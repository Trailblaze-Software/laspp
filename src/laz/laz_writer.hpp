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

#include <span>
#include <sstream>

#include "las_point.hpp"
#include "laz/chunktable.hpp"
#include "laz/encoders.hpp"
#include "laz/point10_encoder.hpp"
#include "laz_vlr.hpp"

namespace laspp {

class LAZWriter {
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
    std::stringstream compressed_data;

    std::vector<LAZEncoder> encoders;
    for (LAZItemRecord record : m_special_vlr.items_records) {
      switch (record.item_type) {
        case LAZItemType::Point10: {
          LASPointFormat0 point;
          if constexpr (is_copy_assignable<decltype(point), T>()) {
            point = points[0];
          }
          encoders.emplace_back(LASPointFormat0Encoder(point));
          compressed_data.write(reinterpret_cast<const char*>(&point), sizeof(LASPointFormat0));
          break;
        }
        case LAZItemType::GPSTime11: {
          GPSTime gps_time;
          if constexpr (is_copy_assignable<decltype(gps_time), T>()) {
            gps_time = points[0];
          }
          encoders.emplace_back(GPSTime11Encoder(gps_time));
          compressed_data.write(reinterpret_cast<const char*>(&gps_time), sizeof(GPSTime));
          break;
        }
        case LAZItemType::Byte: {
          std::vector<std::byte> bytes(record.item_size);
          if constexpr (is_copy_assignable<decltype(bytes), T>()) {
            bytes = points[0];
          }
          LASPP_ASSERT_EQ(record.item_size, bytes.size());
          encoders.emplace_back(BytesEncoder(bytes));
          compressed_data.write(reinterpret_cast<const char*>(bytes.data()),
                                static_cast<int64_t>(bytes.size()));
          break;
        }
        default:
          LASPP_FAIL("Currently unsupported LAZ item type: ",
                     static_cast<uint16_t>(record.item_type));
      }
    }

    {
      OutStream compressed_out_stream(compressed_data);
      std::vector<std::byte> next_bytes;
      for (size_t i = 1; i < points.size(); i++) {
        for (LAZEncoder& laz_encoder : encoders) {
          std::visit(
              [&compressed_out_stream, &points, &i](auto&& encoder) {
                if constexpr (is_copy_assignable<std::remove_const_t<std::remove_reference_t<
                                                     decltype(encoder.last_value())>>,
                                                 T>()) {
                  decltype(encoder.last_value()) value_to_encode = points[i];
                  encoder.encode(compressed_out_stream, value_to_encode);
                }
              },
              laz_encoder);
        }
      }
    }
    return compressed_data;
  }

  template <typename T>
  void write_chunk(const std::span<T>& points) {
    std::stringstream compressed_chunk = compress_chunk(points);
    int64_t compressed_chunk_size = compressed_chunk.tellp();
    LASPP_ASSERT_LT(points.size(), std::numeric_limits<uint32_t>::max());
    LASPP_ASSERT_LT(compressed_chunk_size, std::numeric_limits<uint32_t>::max());
    m_chunk_table.add_chunk(static_cast<uint32_t>(points.size()),
                            static_cast<uint32_t>(compressed_chunk_size));
    m_stream.write(compressed_chunk.str().c_str(), compressed_chunk_size);
    m_special_vlr.chunk_size = m_chunk_table.constant_chunk_size().has_value()
                                   ? m_chunk_table.constant_chunk_size().value()
                                   : std::numeric_limits<uint32_t>::max();
  }

  ~LAZWriter() {
    int64_t chunk_table_offset = m_stream.tellp();
    m_chunk_table.write(m_stream);
    std::cout << "Wrote chunktable: " << m_chunk_table << " at offset " << chunk_table_offset
              << std::endl;
    m_stream.seekp(m_initial_stream_offset);
    m_stream.write(reinterpret_cast<const char*>(&chunk_table_offset), sizeof(chunk_table_offset));
    m_stream.seekp(0, std::ios::end);
  }
};

}  // namespace laspp
