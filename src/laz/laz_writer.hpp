/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <future>
#include <memory>
#include <numeric>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "las_point.hpp"
#include "laz/chunktable.hpp"
#include "laz/encoders.hpp"
#include "laz/layered_stream.hpp"
#include "laz/laz_reader.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/raw_bytes_encoder.hpp"
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"
#include "laz/rgbnir14_encoder.hpp"
#include "laz_vlr.hpp"

namespace laspp {

class LAZWriter {
 public:
  LAZWriter(const LAZWriter&) = delete;
  LAZWriter& operator=(const LAZWriter&) = delete;
  LAZWriter(LAZWriter&&) = delete;
  LAZWriter& operator=(LAZWriter&&) = delete;

 private:
  LAZSpecialVLRContent m_special_vlr;
  LAZChunkTable m_chunk_table;
  std::iostream& m_stream;
  int64_t m_initial_stream_offset;

 public:
  template <typename... Args>
  explicit LAZWriter(std::iostream& stream, Args... laz_special_vlr_args)
      : m_special_vlr(std::forward<Args>(laz_special_vlr_args)...),
        m_stream(stream),
        m_initial_stream_offset(stream.tellp()) {
    int64_t chunk_table_offset = -1;  // Come back and overwrite this later
    m_stream.write(reinterpret_cast<const char*>(&chunk_table_offset), sizeof(chunk_table_offset));
  }

  const LAZSpecialVLRContent& special_vlr() const { return m_special_vlr; }
  LAZSpecialVLRContent& special_vlr() { return m_special_vlr; }

  template <typename T>
  std::stringstream compress_chunk(const std::span<T>& points) {
    LASPP_ASSERT_GT(points.size(), 0);
    std::stringstream compressed_data;

    bool layered_compression = m_special_vlr.compressor == LAZCompressor::LayeredChunked;

    // Byte14 items: one LayeredOutStreams<1> per extra-byte slot, stored as a vector.
    using Byte14OutStreams = std::vector<std::unique_ptr<LayeredOutStreams<1>>>;

    std::vector<LAZEncoder> encoders;
    std::vector<
        std::variant<std::unique_ptr<LayeredOutStreams<1>>, std::unique_ptr<LayeredOutStreams<2>>,
                     std::unique_ptr<LayeredOutStreams<9>>, Byte14OutStreams>>
        layered_streams;
    std::vector<size_t> encoder_num_layers;
    size_t total_layer_count = 0;
    {
      std::optional<uint8_t> context;
      for (LAZItemRecord record : m_special_vlr.items_records) {
        switch (record.item_type) {
          case LAZItemType::Point10: {
            LASPointFormat0 point;
            if constexpr (is_copy_assignable<decltype(point), T>()) {
              point = points[0];
            }
            if (layered_compression) {
              LASPP_FAIL("Cannot use Point10 encoder with layered compression");
            }
            encoders.emplace_back(std::make_unique<LASPointFormat0Encoder>(point));
            compressed_data.write(reinterpret_cast<const char*>(&point), sizeof(LASPointFormat0));
            break;
          }
          case LAZItemType::Point14: {
            LASPointFormat6 point;
            if constexpr (is_copy_assignable<decltype(point), T>()) {
              point = points[0];
            }
            if (record.item_version == LAZItemVersion::Version4) {
              encoders.emplace_back(std::make_unique<LASPointFormat6EncoderV4>(point));
              context = std::get<std::unique_ptr<LASPointFormat6EncoderV4>>(encoders.back())
                            ->get_active_context();
            } else {
              encoders.emplace_back(std::make_unique<LASPointFormat6EncoderV3>(point));
              context = std::get<std::unique_ptr<LASPointFormat6EncoderV3>>(encoders.back())
                            ->get_active_context();
            }
            compressed_data.write(reinterpret_cast<const char*>(&point), sizeof(LASPointFormat6));
            layered_streams.emplace_back(
                std::make_unique<LayeredOutStreams<LASPointFormat6EncoderV4::NUM_LAYERS>>());
            encoder_num_layers.push_back(LASPointFormat6EncoderV4::NUM_LAYERS);
            total_layer_count += LASPointFormat6EncoderV4::NUM_LAYERS;
            break;
          }
          case LAZItemType::GPSTime11: {
            GPSTime gps_time{};
            if constexpr (is_copy_assignable<decltype(gps_time), T>()) {
              gps_time = points[0];
            }
            if (layered_compression) {
              LASPP_FAIL("Cannot use GPSTime11 encoder with layered compression");
            }
            encoders.emplace_back(std::make_unique<GPSTime11Encoder>(gps_time));
            compressed_data.write(reinterpret_cast<const char*>(&gps_time), sizeof(GPSTime));
            break;
          }
          case LAZItemType::RGB12: {
            ColorData color_data{};
            if constexpr (is_copy_assignable<decltype(color_data), T>()) {
              color_data = points[0];
            }
            if (layered_compression) {
              LASPP_FAIL("Cannot use RGB12 encoder with layered compression");
            }
            const bool use_v2 = (record.item_version == LAZItemVersion::Version2);
            if (use_v2) {
              encoders.emplace_back(std::make_unique<RGB12EncoderV2>(color_data));
            } else {
              encoders.emplace_back(std::make_unique<RGB12EncoderV1>(color_data));
            }
            compressed_data.write(reinterpret_cast<const char*>(&color_data), sizeof(ColorData));
            break;
          }
          case LAZItemType::RGB14: {
            ColorData color_data{};
            if constexpr (is_copy_assignable<decltype(color_data), T>()) {
              color_data = points[0];
            }
            LASPP_ASSERT(context.has_value(),
                         "RGB14 requires Point14-derived context; ensure item records are ordered "
                         "so Point14 runs before RGB14.");
            encoders.emplace_back(
                std::make_unique<RGB14Encoder>(color_data, context.value(), true));
            compressed_data.write(reinterpret_cast<const char*>(&color_data), sizeof(ColorData));
            layered_streams.emplace_back(
                std::make_unique<LayeredOutStreams<RGB14Encoder::NUM_LAYERS>>());
            encoder_num_layers.push_back(RGB14Encoder::NUM_LAYERS);
            total_layer_count += RGB14Encoder::NUM_LAYERS;
            break;
          }
          case LAZItemType::Byte: {
            std::vector<std::byte> bytes(record.item_size);
            if constexpr (is_copy_assignable<decltype(bytes), T>()) {
              bytes = points[0];
            }
            LASPP_ASSERT_EQ(record.item_size, bytes.size());
            if (layered_compression) {
              LASPP_FAIL("Cannot use Byte encoder with layered compression");
            }
            encoders.emplace_back(std::make_unique<BytesEncoder>(bytes));
            compressed_data.write(reinterpret_cast<const char*>(bytes.data()),
                                  static_cast<int64_t>(bytes.size()));
            break;
          }
          case LAZItemType::Byte14: {
            // One Byte14Encoder per extra-byte slot; one LayeredOutStreams<1> per slot.
            std::vector<std::byte> bytes(record.item_size, std::byte{0});
            if constexpr (is_copy_assignable<std::vector<std::byte>, T>()) {
              bytes = points[0];
            } else if constexpr (is_copy_fromable<std::vector<std::byte>, T>()) {
              copy_from(bytes, points[0]);
            }
            LASPP_ASSERT_EQ(record.item_size, bytes.size());
            size_t n_extra = record.item_size;
            LASPP_ASSERT(context.has_value(),
                         "Byte14 requires Point14-derived context; ensure item records are "
                         "ordered so Point14 runs before Byte14.");

            std::vector<Byte14Encoder> byte14_encoders;
            byte14_encoders.reserve(n_extra);
            for (size_t j = 0; j < n_extra; j++) {
              byte14_encoders.emplace_back(bytes[j], context.value());
            }
            encoders.emplace_back(std::move(byte14_encoders));
            compressed_data.write(reinterpret_cast<const char*>(bytes.data()),
                                  static_cast<int64_t>(bytes.size()));

            Byte14OutStreams b14_streams;
            b14_streams.reserve(n_extra);
            for (size_t j = 0; j < n_extra; j++) {
              b14_streams.emplace_back(std::make_unique<LayeredOutStreams<1>>());
            }
            layered_streams.emplace_back(std::move(b14_streams));
            encoder_num_layers.push_back(n_extra);
            total_layer_count += n_extra;
            break;
          }
          case LAZItemType::RGBNIR14: {
            RGBNIRData rgbnir{};
            // Populate RGB and NIR independently so user-defined point types only
            // need to support ColorData and/or NIRData conversions (not RGBNIRData).
            using PointT = std::remove_cv_t<T>;
            if constexpr (is_copy_fromable<ColorData, PointT>()) {
              copy_from(rgbnir.rgb, points[0]);
            } else if constexpr (is_copy_assignable<ColorData, PointT>()) {
              rgbnir.rgb = points[0];
            } else if constexpr (std::is_base_of_v<ColorData, PointT>) {
              rgbnir.rgb = static_cast<const ColorData&>(points[0]);
            }

            if constexpr (is_copy_fromable<NIRData, PointT>()) {
              NIRData nir{};
              copy_from(nir, points[0]);
              rgbnir.nir = nir.NIR;
            } else if constexpr (is_copy_assignable<NIRData, PointT>()) {
              NIRData nir = points[0];
              rgbnir.nir = nir.NIR;
            } else if constexpr (std::is_base_of_v<NIRData, PointT>) {
              rgbnir.nir = static_cast<const NIRData&>(points[0]).NIR;
            }
            LASPP_ASSERT(context.has_value(),
                         "RGBNIR14 requires Point14-derived context; ensure item records are "
                         "ordered so Point14 runs before RGBNIR14.");
            encoders.emplace_back(std::make_unique<RGBNIR14Encoder>(rgbnir, context.value(), true));
            compressed_data.write(reinterpret_cast<const char*>(&rgbnir.rgb), sizeof(ColorData));
            compressed_data.write(reinterpret_cast<const char*>(&rgbnir.nir), sizeof(uint16_t));
            layered_streams.emplace_back(
                std::make_unique<LayeredOutStreams<RGBNIR14Encoder::NUM_LAYERS>>());
            encoder_num_layers.push_back(RGBNIR14Encoder::NUM_LAYERS);
            total_layer_count += RGBNIR14Encoder::NUM_LAYERS;
            break;
          }
          case LAZItemType::Short:
          case LAZItemType::Integer:
          case LAZItemType::Long:
          case LAZItemType::Float:
          case LAZItemType::Double: {
            std::vector<std::byte> bytes(record.item_size, std::byte{0});
            if constexpr (is_copy_assignable<std::vector<std::byte>, T>()) {
              bytes = points[0];
            } else if constexpr (is_copy_fromable<std::vector<std::byte>, T>()) {
              copy_from(bytes, points[0]);
            }
            if (layered_compression) {
              LASPP_FAIL("Cannot use RawBytes encoder with layered compression");
            }
            encoders.emplace_back(std::make_unique<RawBytesEncoder>(bytes));
            compressed_data.write(reinterpret_cast<const char*>(bytes.data()),
                                  static_cast<int64_t>(bytes.size()));
            break;
          }
          case LAZItemType::Wavepacket13:
          case LAZItemType::Wavepacket14:
          default:
            LASPP_FAIL("Currently unsupported LAZ item type: ",
                       static_cast<uint16_t>(record.item_type));
        }
      }
    }
    if (layered_compression) {
      LASPP_ASSERT_EQ(encoders.size(), layered_streams.size());
      LASPP_ASSERT_EQ(encoders.size(), encoder_num_layers.size());
    } else {
      LASPP_ASSERT_EQ(layered_streams.size(), 0);
      LASPP_ASSERT_EQ(encoder_num_layers.size(), 0);
    }

    std::vector<uint32_t> layer_sizes;
    std::string layer_payload;

    {
      std::unique_ptr<OutStream> compressed_out_stream;
      if (!layered_compression) {
        compressed_out_stream = std::make_unique<OutStream>(compressed_data);
      }

      for (size_t i = 1; i < points.size(); i++) {
        std::optional<uint8_t> context;
        for (size_t encoder_index = 0; encoder_index < encoders.size(); encoder_index++) {
          LAZEncoder& laz_encoder = encoders[encoder_index];
          std::visit(
              [&](auto&& enc) {
                using ET = std::decay_t<decltype(enc)>;
                if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                  LASPP_ASSERT(layered_compression);
                  auto& b14_streams = std::get<Byte14OutStreams>(layered_streams[encoder_index]);
                  std::vector<std::byte> bytes_to_encode(enc.size());
                  for (size_t j = 0; j < enc.size(); j++) bytes_to_encode[j] = enc[j].last_value();
                  if constexpr (is_copy_assignable<std::vector<std::byte>, T>()) {
                    bytes_to_encode = points[i];
                  } else if constexpr (is_copy_fromable<std::vector<std::byte>, T>()) {
                    copy_from(bytes_to_encode, points[i]);
                  }
                  LASPP_ASSERT_EQ(bytes_to_encode.size(), enc.size());
                  LASPP_ASSERT(context.has_value(),
                               "Byte14 encode requires Point14-derived context; ensure item "
                               "records are ordered so Point14 runs before Byte14.");
                  for (size_t j = 0; j < enc.size(); j++) {
                    enc[j].encode(*b14_streams[j], bytes_to_encode[j], context.value());
                  }
                } else if constexpr (std::is_same_v<std::decay_t<decltype(*enc)>,
                                                    RGBNIR14Encoder>) {
                  LASPP_ASSERT(layered_compression);
                  auto& encoder = *enc;
                  RGBNIRData last_value = encoder.last_value();
                  using PointT = std::remove_cv_t<T>;
                  if constexpr (is_copy_fromable<ColorData, PointT>()) {
                    copy_from(last_value.rgb, points[i]);
                  } else if constexpr (is_copy_assignable<ColorData, PointT>()) {
                    last_value.rgb = points[i];
                  } else if constexpr (std::is_base_of_v<ColorData, PointT>) {
                    last_value.rgb = static_cast<const ColorData&>(points[i]);
                  }

                  if constexpr (is_copy_fromable<NIRData, PointT>()) {
                    NIRData nir{};
                    copy_from(nir, points[i]);
                    last_value.nir = nir.NIR;
                  } else if constexpr (is_copy_assignable<NIRData, PointT>()) {
                    NIRData nir = points[i];
                    last_value.nir = nir.NIR;
                  } else if constexpr (std::is_base_of_v<NIRData, PointT>) {
                    last_value.nir = static_cast<const NIRData&>(points[i]).NIR;
                  }
                  auto& streams =
                      *std::get<std::unique_ptr<LayeredOutStreams<RGBNIR14Encoder::NUM_LAYERS>>>(
                          layered_streams[encoder_index]);
                  encoder.encode(streams, last_value, context.value());
                } else if constexpr (is_copy_assignable<std::remove_const_t<std::remove_reference_t<
                                                            decltype(enc->last_value())>>,
                                                        T>()) {
                  auto& encoder = *enc;
                  using EncoderType = std::remove_reference_t<decltype(encoder)>;
                  decltype(encoder.last_value()) last_value = points[i];
                  if constexpr (std::is_same_v<EncoderType, LASPointFormat6EncoderV3> ||
                                std::is_same_v<EncoderType, LASPointFormat6EncoderV4>) {
                    LASPP_ASSERT(layered_compression);
                    auto& streams =
                        *std::get<std::unique_ptr<LayeredOutStreams<EncoderType::NUM_LAYERS>>>(
                            layered_streams[encoder_index]);
                    encoder.encode(streams, last_value);
                    context = encoder.get_active_context();
                  } else if constexpr (std::is_same_v<EncoderType, RGB14Encoder>) {
                    LASPP_ASSERT(layered_compression);
                    auto& streams =
                        *std::get<std::unique_ptr<LayeredOutStreams<RGB14Encoder::NUM_LAYERS>>>(
                            layered_streams[encoder_index]);
                    encoder.encode(streams, last_value, context.value());
                  } else {
                    LASPP_ASSERT(!layered_compression);
                    LASPP_ASSERT(compressed_out_stream != nullptr);
                    encoder.encode(*compressed_out_stream, last_value);
                  }
                }
              },
              laz_encoder);
        }
      }
    }

    if (total_layer_count > 0) {
      layer_sizes.reserve(total_layer_count);
      for (size_t encoder_index = 0; encoder_index < encoders.size(); encoder_index++) {
        std::visit(
            [&](auto&& enc) {
              using ET = std::decay_t<decltype(enc)>;
              if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                auto& b14_streams = std::get<Byte14OutStreams>(layered_streams[encoder_index]);
                for (auto& s : b14_streams) {
                  auto sizes = s->layer_sizes();
                  layer_sizes.push_back(sizes[0]);
                  layer_payload += s->cb().str();
                }
              } else {
                auto& encoder = *enc;
                using EncoderType = std::remove_reference_t<decltype(encoder)>;
                if constexpr (has_num_layers_v<EncoderType>) {
                  auto& streams =
                      *std::get<std::unique_ptr<LayeredOutStreams<EncoderType::NUM_LAYERS>>>(
                          layered_streams[encoder_index]);
                  auto sizes = streams.layer_sizes();
                  for (uint32_t size : sizes) layer_sizes.push_back(size);
                  layer_payload += streams.cb().str();
                }
              }
            },
            encoders[encoder_index]);
      }
    }

    if (layered_compression) {
      LASPP_ASSERT_EQ(total_layer_count, layer_sizes.size());
      std::string seed_data = compressed_data.str();
      compressed_data.str(std::string());
      compressed_data.clear();

      compressed_data.write(seed_data.data(), static_cast<std::streamsize>(seed_data.size()));
      uint32_t num_points = static_cast<uint32_t>(points.size());
      compressed_data.write(reinterpret_cast<const char*>(&num_points), sizeof(uint32_t));
      for (uint32_t size : layer_sizes) {
        compressed_data.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
      }
      compressed_data.write(layer_payload.data(),
                            static_cast<std::streamsize>(layer_payload.size()));
    } else {
      LASPP_ASSERT_EQ(total_layer_count, 0);
    }
    return compressed_data;
  }

  template <typename T>
  void write_chunk(const std::span<T>& points, std::optional<uint32_t> index = std::nullopt) {
    std::stringstream compressed_chunk = compress_chunk(points);
    int64_t compressed_chunk_size = compressed_chunk.tellp();
    LASPP_ASSERT_LT(points.size(), std::numeric_limits<uint32_t>::max());
    LASPP_ASSERT_LT(compressed_chunk_size, std::numeric_limits<uint32_t>::max());
    if (index.has_value()) {
      while (index.value() > m_chunk_table.num_chunks()) {
      }
    }
    m_chunk_table.add_chunk(static_cast<uint32_t>(points.size()),
                            static_cast<uint32_t>(compressed_chunk_size));
    m_stream.write(compressed_chunk.str().c_str(), compressed_chunk_size);
    m_special_vlr.chunk_size = m_chunk_table.constant_chunk_size().has_value()
                                   ? m_chunk_table.constant_chunk_size().value()
                                   : std::numeric_limits<uint32_t>::max();
  }

  template <typename T>
  void write_chunks(const std::span<std::span<T>> chunks) {
    // Pipeline: launch all compressions asynchronously, then write chunks in order
    // as soon as each one completes. This overlaps compression (CPU) with I/O.
    struct ChunkResult {
      std::string payload;
      uint32_t compressed_size;
      uint32_t points_count;
    };

    std::vector<std::future<ChunkResult>> compression_futures;
    compression_futures.reserve(chunks.size());

    for (size_t i = 0; i < chunks.size(); i++) {
      compression_futures.push_back(
          std::async(std::launch::async, [this, chunk = chunks[i]]() -> ChunkResult {
            std::stringstream compressed_chunk = compress_chunk(chunk);
            int64_t compressed_chunk_size = compressed_chunk.tellp();
            LASPP_ASSERT_LT(chunk.size(), std::numeric_limits<uint32_t>::max());
            LASPP_ASSERT_LT(compressed_chunk_size, std::numeric_limits<uint32_t>::max());

            ChunkResult result;
            result.payload = compressed_chunk.str();
            result.compressed_size = static_cast<uint32_t>(compressed_chunk_size);
            result.points_count = static_cast<uint32_t>(chunk.size());
            return result;
          }));
    }

    for (size_t i = 0; i < chunks.size(); i++) {
      ChunkResult result = compression_futures[i].get();
      m_chunk_table.add_chunk(result.points_count, result.compressed_size);
      m_stream.write(result.payload.data(), static_cast<std::streamsize>(result.compressed_size));
      m_special_vlr.chunk_size = m_chunk_table.constant_chunk_size().has_value()
                                     ? m_chunk_table.constant_chunk_size().value()
                                     : std::numeric_limits<uint32_t>::max();
    }
  }

  ~LAZWriter() {
    int64_t chunk_table_offset = m_stream.tellp();
    m_chunk_table.write(m_stream);
    m_stream.seekp(m_initial_stream_offset);
    m_stream.write(reinterpret_cast<const char*>(&chunk_table_offset), sizeof(chunk_table_offset));
    m_stream.seekp(0, std::ios::end);
  }
};

}  // namespace laspp
