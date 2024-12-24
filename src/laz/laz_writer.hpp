/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#pragma once

#include <span>
#include <sstream>

#include "laz/chunktable.hpp"
#include "laz/encoders.hpp"
#include "laz/point10_encoder.hpp"
#include "laz_vlr.hpp"

namespace laspp {

class LAZWriter {
  LAZSpecialVLR m_special_vlr;
  LAZChunkTable m_chunk_table;
  std::iostream& m_stream;
  int64_t m_initial_stream_offset;

 public:
  template <typename... Args>
  explicit LAZWriter(std::iostream& stream, Args... args)
      : m_special_vlr(std::forward<Args>(args)...),
        m_stream(stream),
        m_initial_stream_offset(stream.tellp()) {
    int64_t chunk_table_offset = -1;  // Come back and overwrite this later
    m_stream.write(reinterpret_cast<const char*>(&chunk_table_offset), sizeof(chunk_table_offset));
  }

  const LAZSpecialVLR& special_vlr() const { return m_special_vlr; }

  template <typename T>
  std::stringstream compress_chunk(std::span<T> points) {
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

    OutStream compressed_out_stream(compressed_data);
    std::vector<std::byte> next_bytes;
    for (size_t i = 1; i < points.size(); i++) {
      for (LAZEncoder& laz_encoder : encoders) {
        std::visit(
            [&compressed_out_stream, &points, &i](auto&& encoder) {
              if constexpr (is_copy_assignable<T, decltype(encoder.last_value())>()) {
                decltype(encoder.last_value()) value_to_encode = points[i];
                encoder.encode(compressed_out_stream, value_to_encode);
              }
            },
            laz_encoder);
      }
    }
    return compressed_data;
  }

  ~LAZWriter() {
    int64_t chunk_table_offset = m_stream.tellp();
    std::cout << "Writing chunk table: " << m_chunk_table << std::endl;
    std::cout << "Chunk table offset: " << chunk_table_offset << " " << m_stream.tellp()
              << std::endl;
    m_chunk_table.write(m_stream);
    std::cout << "Chunk table offset: " << chunk_table_offset << " " << m_stream.tellp()
              << std::endl;
    m_stream.seekp(m_initial_stream_offset);
    m_stream.write(reinterpret_cast<const char*>(&chunk_table_offset), sizeof(chunk_table_offset));
    m_stream.seekp(0, std::ios::end);
  }
};

}  // namespace laspp
