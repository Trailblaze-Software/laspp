/*
 * SPDX-FileCopyrightText: (c) 2025 Trailblaze Software, all rights reserved
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
#include <span>

#include "chunktable.hpp"
#include "las_point.hpp"
#include "laz/byte_encoder.hpp"
#include "laz/encoders.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/rgb12_encoder.hpp"
#include "laz/stream.hpp"
#include "laz_vlr.hpp"
#include "utilities/assert.hpp"

namespace laspp {

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
    LASPP_ASSERT(m_chunk_table.has_value());

    std::vector<LAZEncoder> encoders;
    for (LAZItemRecord record : m_special_vlr.items_records) {
      switch (record.item_type) {
        case LAZItemType::Point10: {
          encoders.push_back(
              LASPointFormat0Encoder(*reinterpret_cast<LASPointFormat0*>(compressed_data.data())));
          compressed_data = compressed_data.subspan(sizeof(LASPointFormat0));
          break;
        }
          // case LAZItemType::Point14: {
          // encoders.push_back(
          // LASPointFormat6Encoder(*reinterpret_cast<LASPointFormat6*>(compressed_data.data())));
          // compressed_data = compressed_data.subspan(sizeof(LASPointFormat6));
        //}
        case LAZItemType::GPSTime11: {
          encoders.push_back(GPSTime11Encoder(*reinterpret_cast<GPSTime*>(compressed_data.data())));
          compressed_data = compressed_data.subspan(sizeof(GPSTime));
          break;
        }
        case LAZItemType::RGB12: {
          encoders.push_back(RGB12Encoder(*reinterpret_cast<ColorData*>(compressed_data.data())));
          compressed_data = compressed_data.subspan(sizeof(ColorData));
          break;
        }
        case LAZItemType::Byte: {
          std::vector<std::byte> last_bytes(record.item_size);
          std::copy_n(compressed_data.data(), record.item_size, last_bytes.begin());
          encoders.push_back(BytesEncoder(last_bytes));
          compressed_data = compressed_data.subspan(record.item_size);
          break;
        }
        default:
          LASPP_FAIL("Currently unsupported LAZ item type: ", LAZItemType(record.item_type), " (",
                     static_cast<uint16_t>(record.item_type), ")");
      }
    }

    if (m_special_vlr.compressor == LAZCompressor::LayeredChunked) {
      uint32_t num_points;
      std::copy(compressed_data.begin(), compressed_data.begin() + sizeof(uint32_t),
                reinterpret_cast<std::byte*>(&num_points));
      compressed_data = compressed_data.subspan(sizeof(uint32_t));
      LASPP_ASSERT_EQ(num_points, decompressed_data.size());
    }

    PointerStreamBuffer compressed_buffer(compressed_data.data(), compressed_data.size());
    std::istream compressed_stream(&compressed_buffer);
    InStream compressed_in_stream(compressed_stream);
    for (size_t i = 0; i < decompressed_data.size(); i++) {
      for (LAZEncoder& laz_encoder : encoders) {
        std::visit(
            [&compressed_in_stream, &decompressed_data, &i](auto&& encoder) {
              if (i > 0) encoder.decode(compressed_in_stream);

              if constexpr (is_copy_fromable<T, decltype(encoder.last_value())>()) {
                copy_from(decompressed_data[i], encoder.last_value());
              } else if constexpr (is_copy_assignable<T, decltype(encoder.last_value())>()) {
                decompressed_data[i] = encoder.last_value();
              } else if constexpr (std::is_base_of_v<
                                       std::remove_reference_t<decltype(encoder.last_value())>,
                                       T>) {
                using DecompressedType =
                    std::remove_const_t<std::remove_reference_t<decltype(encoder.last_value())>>;
                static_cast<DecompressedType&>(decompressed_data[i]) = encoder.last_value();
              }
            },
            laz_encoder);
      }
    }
    return decompressed_data;
  }
};

}  // namespace laspp
