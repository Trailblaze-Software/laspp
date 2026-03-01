/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; version 2.1.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <execution>
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
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"
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

    std::vector<LAZEncoder> encoders;
    std::vector<
        std::variant<std::unique_ptr<LayeredOutStreams<1>>, std::unique_ptr<LayeredOutStreams<9>>>>
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
            encoders.emplace_back(std::make_unique<LASPointFormat6Encoder>(point));
            context = std::get<std::unique_ptr<LASPointFormat6Encoder>>(encoders.back())
                          ->get_active_context();
            compressed_data.write(reinterpret_cast<const char*>(&point), sizeof(LASPointFormat6));
            layered_streams.emplace_back(
                std::make_unique<LayeredOutStreams<LASPointFormat6Encoder::NUM_LAYERS>>());
            encoder_num_layers.push_back(LASPointFormat6Encoder::NUM_LAYERS);
            total_layer_count += LASPointFormat6Encoder::NUM_LAYERS;
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
            encoders.emplace_back(std::make_unique<RGB12Encoder>(color_data));
            compressed_data.write(reinterpret_cast<const char*>(&color_data), sizeof(ColorData));
            break;
          }
          case LAZItemType::RGB14: {
            ColorData color_data{};
            if constexpr (is_copy_assignable<decltype(color_data), T>()) {
              color_data = points[0];
            }
            encoders.emplace_back(std::make_unique<RGB14Encoder>(color_data, context.value()));
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
          case LAZItemType::Short:
          case LAZItemType::Integer:
          case LAZItemType::Long:
          case LAZItemType::Float:
          case LAZItemType::Double:
          case LAZItemType::Wavepacket13:
          case LAZItemType::RGBNIR14:
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

      {
        for (size_t i = 1; i < points.size(); i++) {
          std::optional<uint8_t> context;
          for (size_t encoder_index = 0; encoder_index < encoders.size(); encoder_index++) {
            LAZEncoder& laz_encoder = encoders[encoder_index];
            std::visit(
                [&](auto&& enc_ptr) {
                  auto& encoder = *enc_ptr;
                  if constexpr (is_copy_assignable<std::remove_const_t<std::remove_reference_t<
                                                       decltype(encoder.last_value())>>,
                                                   T>()) {
                    decltype(encoder.last_value()) value_to_encode = points[i];
                    using EncoderType = std::decay_t<decltype(encoder)>;
                    if constexpr (std::is_same_v<EncoderType, LASPointFormat6Encoder>) {
                      LASPP_ASSERT(layered_compression);
                      auto& streams = *std::get<
                          std::unique_ptr<LayeredOutStreams<LASPointFormat6Encoder::NUM_LAYERS>>>(
                          layered_streams[encoder_index]);
                      encoder.encode(streams, value_to_encode);
                      context = encoder.get_active_context();
                    } else if constexpr (std::is_same_v<EncoderType, RGB14Encoder>) {
                      LASPP_ASSERT(layered_compression);
                      auto& streams =
                          *std::get<std::unique_ptr<LayeredOutStreams<RGB14Encoder::NUM_LAYERS>>>(
                              layered_streams[encoder_index]);
                      encoder.encode(streams, value_to_encode, context.value());
                    } else {
                      LASPP_ASSERT(!layered_compression);
                      LASPP_ASSERT(compressed_out_stream != nullptr);
                      encoder.encode(*compressed_out_stream, value_to_encode);
                    }
                  }
                },
                laz_encoder);
          }
        }
      }
    }

    if (total_layer_count > 0) {
      layer_sizes.reserve(total_layer_count);
      for (size_t encoder_index = 0; encoder_index < encoders.size(); encoder_index++) {
        std::visit(
            [&](auto&& enc_ptr) {
              auto& encoder = *enc_ptr;
              if constexpr (has_num_layers_v<decltype(encoder)>) {
                auto& streams = *std::get<std::unique_ptr<
                    LayeredOutStreams<std::remove_reference_t<decltype(encoder)>::NUM_LAYERS>>>(
                    layered_streams[encoder_index]);
                auto sizes = streams.layer_sizes();
                for (uint32_t size : sizes) {
                  layer_sizes.push_back(size);
                }
                layer_payload += streams.cb().str();
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

    // Launch all compressions asynchronously
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

    // Write chunks in order as they become ready (pipelined I/O)
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
