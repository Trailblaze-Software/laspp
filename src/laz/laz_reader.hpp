/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

#include "chunktable.hpp"
#include "las_point.hpp"
#include "laz/byte_encoder.hpp"
#include "laz/encoders.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/layered_stream.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/raw_bytes_encoder.hpp"
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"
#include "laz/rgbnir14_encoder.hpp"
#include "laz/stream.hpp"
#include "laz_vlr.hpp"
#include "utilities/assert.hpp"
#include "utilities/macros.hpp"

namespace laspp {

template <typename, typename = void>
struct has_num_layers : std::false_type {};

template <typename T>
struct has_num_layers<
    T, std::void_t<decltype(std::remove_const_t<std::remove_reference_t<T>>::NUM_LAYERS)>>
    : std::true_type {};

template <typename T>
constexpr bool has_num_layers_v = has_num_layers<T>::value;

template <typename TDest, typename TSrc>
void copy_from_if_possible(TDest& dest, const TSrc& src) {
  if constexpr (is_copy_fromable<TDest, TSrc>()) {
    copy_from(dest, src);
  } else if constexpr (is_copy_assignable<TDest, TSrc>()) {
    dest = src;
  } else if constexpr (std::is_base_of_v<std::remove_const_t<std::remove_reference_t<TSrc>>,
                                         TDest>) {
    using DecompressedType = std::remove_const_t<std::remove_reference_t<TSrc>>;
    static_cast<DecompressedType&>(dest) = src;
  }
}

class LAZReader {
  LAZSpecialVLRContent m_special_vlr;
  std::optional<LAZChunkTable> m_chunk_table;

  std::optional<size_t> chunk_size() const {
    if (m_special_vlr.chunk_size == std::numeric_limits<uint32_t>::max()) {
      return std::nullopt;
    }
    return m_special_vlr.chunk_size;
  }

 public:
  explicit LAZReader(const LAZSpecialVLRContent& special_vlr) : m_special_vlr(special_vlr) {}

  void read_chunk_table(std::istream& in_stream, size_t n_points) {
    int64_t chunk_table_offset;
    LASPP_CHECK_READ(in_stream, &chunk_table_offset, sizeof(size_t));
    if (chunk_table_offset == -1) {
      LASPP_UNIMPLEMENTED("Reading chunk table from LAS file");
    }

    LASPP_CHECK_SEEK(in_stream, chunk_table_offset, std::ios::beg);
    m_chunk_table.emplace(LAZChunkTable(in_stream, chunk_size(), n_points));
  }

  const LAZChunkTable& chunk_table() const { return m_chunk_table.value(); }

  template <typename T>
  std::span<T> decompress_chunk(std::span<const std::byte> compressed_data,
                                std::span<T> decompressed_data) {
    std::vector<LAZEncoder> encoders;
    {
      std::optional<uint8_t> context;
      for (LAZItemRecord record : m_special_vlr.items_records) {
        switch (record.item_type) {
          case LAZItemType::Point14: {
            const LASPointFormat6 seed =
                *reinterpret_cast<const LASPointFormat6*>(compressed_data.data());
            if (record.item_version == LAZItemVersion::Version4) {
              encoders.emplace_back(std::make_unique<LASPointFormat6EncoderV4>(seed));
              context = std::get<std::unique_ptr<LASPointFormat6EncoderV4>>(encoders.back())
                            ->get_active_context();
            } else {
              encoders.emplace_back(std::make_unique<LASPointFormat6EncoderV3>(seed));
              context = std::get<std::unique_ptr<LASPointFormat6EncoderV3>>(encoders.back())
                            ->get_active_context();
            }
            compressed_data = compressed_data.subspan(sizeof(LASPointFormat6));
            break;
          }
          case LAZItemType::RGB14: {
            encoders.emplace_back(std::make_unique<RGB14Encoder>(
                *reinterpret_cast<const ColorData*>(compressed_data.data()), context.value(),
                true));
            compressed_data = compressed_data.subspan(sizeof(ColorData));
            break;
          }
          case LAZItemType::Point10: {
            encoders.emplace_back(std::make_unique<LASPointFormat0Encoder>(
                *reinterpret_cast<const LASPointFormat0*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(LASPointFormat0));
            break;
          }
          case LAZItemType::GPSTime11: {
            encoders.emplace_back(std::make_unique<GPSTime11Encoder>(
                *reinterpret_cast<const GPSTime*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(GPSTime));
            break;
          }
          case LAZItemType::RGB12: {
            encoders.emplace_back(std::make_unique<RGB12Encoder>(
                *reinterpret_cast<const ColorData*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(ColorData));
            break;
          }
          case LAZItemType::Byte: {
            std::vector<std::byte> last_bytes(record.item_size);
            std::copy_n(compressed_data.data(), record.item_size, last_bytes.begin());
            encoders.emplace_back(std::make_unique<BytesEncoder>(last_bytes));
            compressed_data = compressed_data.subspan(record.item_size);
            break;
          }
          case LAZItemType::Byte14: {
            // One Byte14Encoder per extra-byte slot, each wired to its own stream.
            std::vector<Byte14Encoder> byte14_encoders;
            byte14_encoders.reserve(record.item_size);
            for (size_t j = 0; j < record.item_size; j++) {
              byte14_encoders.emplace_back(
                  *reinterpret_cast<const std::byte*>(compressed_data.data() + j),
                  context.value_or(0));
            }
            encoders.emplace_back(std::move(byte14_encoders));
            compressed_data = compressed_data.subspan(record.item_size);
            break;
          }
          case LAZItemType::RGBNIR14: {
            // Seed is stored as ColorData (6 bytes) followed by uint16_t NIR (2 bytes).
            RGBNIRData initial{};
            std::memcpy(&initial.rgb, compressed_data.data(), sizeof(ColorData));
            std::memcpy(&initial.nir, compressed_data.data() + sizeof(ColorData), sizeof(uint16_t));
            const bool use_v3_quirk = (record.item_version == LAZItemVersion::Version3);
            encoders.emplace_back(
                std::make_unique<RGBNIR14Encoder>(initial, context.value(), use_v3_quirk));
            compressed_data = compressed_data.subspan(sizeof(ColorData) + sizeof(uint16_t));
            break;
          }
          case LAZItemType::Short:
          case LAZItemType::Integer:
          case LAZItemType::Long:
          case LAZItemType::Float:
          case LAZItemType::Double: {
            std::vector<std::byte> last_bytes(record.item_size);
            std::copy_n(compressed_data.data(), record.item_size, last_bytes.begin());
            encoders.emplace_back(std::make_unique<RawBytesEncoder>(last_bytes));
            compressed_data = compressed_data.subspan(record.item_size);
            break;
          }
          case LAZItemType::Wavepacket13:
          case LAZItemType::Wavepacket14:
          default:
            LASPP_FAIL("Currently unsupported LAZ item type: ", LAZItemType(record.item_type), " (",
                       static_cast<uint16_t>(record.item_type), ")");
        }
      }
    }

    if (m_special_vlr.compressor == LAZCompressor::LayeredChunked) {
      {
        uint32_t num_points;
        std::memcpy(&num_points, compressed_data.data(), sizeof(num_points));
        compressed_data = compressed_data.subspan(sizeof(uint32_t));
        LASPP_ASSERT_EQ(num_points, decompressed_data.size());
      }

      static_assert(has_num_layers<laspp::LASPointFormat6Encoder>::value,
                    "LAZPointFormat6Encoder must have NUM_LAYERS");

      // Count total layers: unique_ptr encoders use their compile-time NUM_LAYERS;
      // vector<Byte14Encoder> contributes one layer per slot.
      size_t total_n_layers = 0;
      for (const LAZEncoder& encoder : encoders) {
        std::visit(
            [&total_n_layers](auto&& enc) {
              using ET = std::decay_t<decltype(enc)>;
              if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                total_n_layers += enc.size();
              } else {
                using EncT = std::decay_t<decltype(*enc)>;
                if constexpr (has_num_layers_v<EncT>) {
                  total_n_layers += EncT::NUM_LAYERS;
                } else {
                  LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
                }
              }
            },
            encoder);
      }
      std::span<const std::byte> compressed_layer_data =
          compressed_data.subspan(total_n_layers * sizeof(uint32_t));

      // Byte14 items: one LayeredInStreams<1> per slot, stored as a vector.
      // Standard items: one LayeredInStreams<N> per encoder.
      using Byte14InStreams = std::vector<std::unique_ptr<LayeredInStreams<1>>>;
      std::vector<
          std::variant<std::unique_ptr<LayeredInStreams<1>>, std::unique_ptr<LayeredInStreams<2>>,
                       std::unique_ptr<LayeredInStreams<9>>, Byte14InStreams>>
          layered_in_streams_for_encoders;
      for (const LAZEncoder& encoder : encoders) {
        std::visit(
            [&compressed_data, &compressed_layer_data,
             &layered_in_streams_for_encoders](auto&& enc) {
              using ET = std::decay_t<decltype(enc)>;
              if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                Byte14InStreams streams;
                streams.reserve(enc.size());
                for (size_t j = 0; j < enc.size(); j++) {
                  streams.emplace_back(std::make_unique<LayeredInStreams<1>>(
                      compressed_data, compressed_layer_data));
                }
                layered_in_streams_for_encoders.emplace_back(std::move(streams));
              } else if constexpr (has_num_layers_v<std::decay_t<decltype(*enc)>>) {
                using EncT = std::decay_t<decltype(*enc)>;
                layered_in_streams_for_encoders.emplace_back(
                    std::make_unique<LayeredInStreams<EncT::NUM_LAYERS>>(compressed_data,
                                                                         compressed_layer_data));
              } else {
                LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
              }
            },
            encoder);
      }
      LASPP_ASSERT_EQ(compressed_layer_data.size(), 0);

      for (size_t i = 0; i < decompressed_data.size(); i++) {
        if (i + 3 < decompressed_data.size()) {
          LASPP_PREFETCH(&decompressed_data[i + 3]);
        }

        std::optional<uint8_t> context;
        for (size_t encoder_idx = 0; encoder_idx < encoders.size(); encoder_idx++) {
          LAZEncoder& laz_encoder = encoders[encoder_idx];

          std::visit(
              [&decompressed_data, &i, &layered_in_streams_for_encoders, encoder_idx,
               &context](auto&& enc) {
                using ET = std::decay_t<decltype(enc)>;
                if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                  auto& streams =
                      std::get<Byte14InStreams>(layered_in_streams_for_encoders[encoder_idx]);
                  if (i > 0) {
                    for (size_t j = 0; j < enc.size(); j++) {
                      enc[j].decode(*streams[j], context.value());
                    }
                  }
                  std::vector<std::byte> all_bytes(enc.size());
                  for (size_t j = 0; j < enc.size(); j++) all_bytes[j] = enc[j].last_value();
                  copy_from_if_possible(decompressed_data[i], all_bytes);
                } else {
                  auto& encoder = *enc;
                  using EncType = std::remove_reference_t<decltype(encoder)>;
                  if constexpr (has_num_layers_v<EncType>) {
                    LayeredInStreams<EncType::NUM_LAYERS>& layered_in_stream =
                        *std::get<std::unique_ptr<LayeredInStreams<EncType::NUM_LAYERS>>>(
                            layered_in_streams_for_encoders[encoder_idx]);
                    if constexpr (std::is_same_v<EncType, LASPointFormat6EncoderV3> ||
                                  std::is_same_v<EncType, LASPointFormat6EncoderV4>) {
                      if (i > 0) {
                        auto decoded_val = encoder.decode(layered_in_stream);
                        context = encoder.get_active_context();
                        copy_from_if_possible(decompressed_data[i], decoded_val);
                        return;
                      }
                    } else {
                      if (i > 0) {
                        auto decoded_val = encoder.decode(layered_in_stream, context.value());
                        copy_from_if_possible(decompressed_data[i], decoded_val);
                        return;
                      }
                    }
                    copy_from_if_possible(decompressed_data[i], encoder.last_value());
                  } else {
                    LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
                  }
                }
              },
              laz_encoder);
        }
      }
    } else {
      InStream compressed_in_stream(compressed_data.data(), compressed_data.size());
      for (size_t i = 0; i < decompressed_data.size(); i++) {
        for (LAZEncoder& laz_encoder : encoders) {
          std::visit(
              [&compressed_in_stream, &decompressed_data, &i](auto&& enc) {
                using ET = std::decay_t<decltype(enc)>;
                if constexpr (std::is_same_v<ET, std::vector<Byte14Encoder>>) {
                  LASPP_FAIL("Cannot use layered encoder with non-layered compression.");
                } else {
                  auto& encoder = *enc;
                  using EncType = std::remove_reference_t<decltype(encoder)>;
                  if constexpr (has_num_layers_v<EncType>) {
                    LASPP_FAIL("Cannot use layered encoder with non-layered compression.");
                  } else {
                    if (i > 0) encoder.decode(compressed_in_stream);
                    copy_from_if_possible(decompressed_data[i], encoder.last_value());
                  }
                }
              },
              laz_encoder);
        }
      }
    }
    return decompressed_data;
  }
};

}  // namespace laspp
