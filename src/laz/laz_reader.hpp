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
#include <cstdint>
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
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"
#include "laz/stream.hpp"
#include "laz_vlr.hpp"
#include "utilities/assert.hpp"

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
    LASPP_CHECK_READ(in_stream.read(reinterpret_cast<char*>(&chunk_table_offset),
                                    static_cast<int64_t>(sizeof(size_t))));
    if (chunk_table_offset == -1) {
      LASPP_UNIMPLEMENTED("Reading chunk table from LAS file");
    }

    in_stream.seekg(chunk_table_offset);

    m_chunk_table.emplace(LAZChunkTable(in_stream, chunk_size(), n_points));
  }

  const LAZChunkTable& chunk_table() const { return m_chunk_table.value(); }

  template <typename T>
  std::span<T> decompress_chunk(std::span<std::byte> compressed_data,
                                std::span<T> decompressed_data) {
    std::vector<LAZEncoder> encoders;
    {
      std::optional<uint8_t> context;
      for (LAZItemRecord record : m_special_vlr.items_records) {
        switch (record.item_type) {
          case LAZItemType::Point14: {
            encoders.emplace_back(LASPointFormat6Encoder(
                *reinterpret_cast<LASPointFormat6*>(compressed_data.data())));
            context = std::get<LASPointFormat6Encoder>(encoders.back()).get_active_context();
            compressed_data = compressed_data.subspan(sizeof(LASPointFormat6));
            break;
          }
          case LAZItemType::RGB14: {
            encoders.emplace_back(RGB14Encoder(
                *reinterpret_cast<ColorData*>(compressed_data.data()), context.value()));
            compressed_data = compressed_data.subspan(sizeof(ColorData));
            break;
          }
          case LAZItemType::Point10: {
            encoders.emplace_back(LASPointFormat0Encoder(
                *reinterpret_cast<LASPointFormat0*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(LASPointFormat0));
            break;
          }
          case LAZItemType::GPSTime11: {
            encoders.emplace_back(
                GPSTime11Encoder(*reinterpret_cast<GPSTime*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(GPSTime));
            break;
          }
          case LAZItemType::RGB12: {
            encoders.emplace_back(
                RGB12Encoder(*reinterpret_cast<ColorData*>(compressed_data.data())));
            compressed_data = compressed_data.subspan(sizeof(ColorData));
            break;
          }
          case LAZItemType::Byte: {
            std::vector<std::byte> last_bytes(record.item_size);
            std::copy_n(compressed_data.data(), record.item_size, last_bytes.begin());
            encoders.emplace_back(BytesEncoder(last_bytes));
            compressed_data = compressed_data.subspan(record.item_size);
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
      size_t total_n_layers = 0;
      for (const LAZEncoder& encoder : encoders) {
        std::visit(
            [&total_n_layers](auto&& enc) {
              if constexpr (has_num_layers_v<decltype(enc)>) {
                total_n_layers += std::remove_reference_t<decltype(enc)>::NUM_LAYERS;
              } else {
                LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
              }
            },
            encoder);
      }
      std::span<std::byte> compressed_layer_data =
          compressed_data.subspan(total_n_layers * sizeof(uint32_t));

      std::vector<
          std::variant<std::unique_ptr<LayeredInStreams<1>>, std::unique_ptr<LayeredInStreams<9>>>>
          layered_in_streams_for_encoders;
      for (const LAZEncoder& encoder : encoders) {
        std::visit(
            [&compressed_data, &compressed_layer_data,
             &layered_in_streams_for_encoders](auto&& enc) {
              if constexpr (has_num_layers_v<decltype(enc)>) {
                layered_in_streams_for_encoders.emplace_back(
                    std::make_unique<
                        LayeredInStreams<std::remove_reference_t<decltype(enc)>::NUM_LAYERS>>(
                        compressed_data, compressed_layer_data));
              } else {
                LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
              }
            },
            encoder);
      }
      LASPP_ASSERT_EQ(compressed_layer_data.size(), 0);
      for (size_t i = 0; i < decompressed_data.size(); i++) {
        std::optional<uint8_t> context;
        for (size_t encoder_idx = 0; encoder_idx < encoders.size(); encoder_idx++) {
          LAZEncoder& laz_encoder = encoders[encoder_idx];
          std::visit(
              [&decompressed_data, &i, &layered_in_streams_for_encoders, &encoder_idx,
               &context](auto&& encoder) {
                if constexpr (has_num_layers_v<decltype(encoder)>) {
                  LayeredInStreams<std::remove_reference_t<decltype(encoder)>::NUM_LAYERS>&
                      layered_in_stream = *std::get<std::unique_ptr<LayeredInStreams<
                          std::remove_reference_t<decltype(encoder)>::NUM_LAYERS>>>(
                          layered_in_streams_for_encoders[encoder_idx]);
                  if (i > 0) {
                    if constexpr (std::is_same_v<std::remove_reference_t<decltype(encoder)>,
                                                 LASPointFormat6Encoder>) {
                      encoder.decode(layered_in_stream);
                      context = encoder.get_active_context();
                    } else {
                      encoder.decode(layered_in_stream, context.value());
                    }
                  }
                  copy_from_if_possible(decompressed_data[i], encoder.last_value());
                } else {
                  LASPP_FAIL("Cannot use layered decompression with non-layered encoder.");
                }
              },
              laz_encoder);
        }
      }
    } else {
      PointerStreamBuffer compressed_buffer(compressed_data.data(), compressed_data.size());
      std::istream compressed_stream(&compressed_buffer);
      InStream compressed_in_stream(compressed_stream);
      for (size_t i = 0; i < decompressed_data.size(); i++) {
        for (LAZEncoder& laz_encoder : encoders) {
          std::visit(
              [&compressed_in_stream, &decompressed_data, &i](auto&& encoder) {
                if constexpr (has_num_layers_v<decltype(encoder)>) {
                  LASPP_FAIL("Cannot use layered encoder with non-layered compression.");
                } else {
                  if (i > 0) encoder.decode(compressed_in_stream);
                  copy_from_if_possible(decompressed_data[i], encoder.last_value());
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
