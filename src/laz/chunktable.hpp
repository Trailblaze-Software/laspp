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

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/assert.hpp"
#include "utilities/macros.hpp"
#include "utilities/printing.hpp"

namespace laspp {

#pragma pack(push, 1)

struct LASPP_PACKED LAZChunkTableHeader {
  uint32_t version = 0;  // 0
  uint32_t number_of_chunks = 0;

  friend std::ostream& operator<<(std::ostream& os, const LAZChunkTableHeader& chunk_header) {
    os << "Version: " << chunk_header.version << std::endl;
    os << "Number of chunks: " << chunk_header.number_of_chunks << std::endl;
    return os;
  }
};

class LAZChunkTable : LAZChunkTableHeader {
  std::vector<uint32_t> m_compressed_chunk_size;
  std::optional<uint32_t> m_constant_chunk_size;
  std::vector<size_t> m_n_points_per_chunk;
  std::vector<size_t> m_compressed_chunk_offsets;
  std::vector<size_t> m_decompressed_chunk_offsets;

 public:
  explicit LAZChunkTable(std::istream& istream,
                         std::optional<uint32_t> constant_chunk_size = std::nullopt,
                         size_t total_n_points = 0)
      : m_constant_chunk_size(constant_chunk_size) {
    LASPP_CHECK_READ(istream.read(reinterpret_cast<char*>(this),
                                  static_cast<int64_t>(sizeof(LAZChunkTableHeader))));
    if (constant_chunk_size) {
      if (constant_chunk_size.value() == 0) {
        LASPP_ASSERT_EQ(0, number_of_chunks);
      } else {
        LASPP_ASSERT_EQ(
            (total_n_points + constant_chunk_size.value() - 1) / constant_chunk_size.value(),
            number_of_chunks);
      }
    }

    InStream decoder(istream);
    IntegerEncoder<32> int_decoder;
    for (size_t i = 0; i < number_of_chunks; i++) {
      uint32_t decoded_int = static_cast<uint32_t>(int_decoder.decode_int(decoder));
      uint32_t previous_int = i == 0 ? 0u : m_compressed_chunk_size[i - 1];
      m_compressed_chunk_size.push_back(decoded_int + previous_int);
      m_compressed_chunk_offsets.push_back(
          i == 0 ? 8 : m_compressed_chunk_offsets[i - 1] + m_compressed_chunk_size[i - 1]);
      if (m_constant_chunk_size) {
        m_n_points_per_chunk.push_back(i == number_of_chunks - 1
                                           ? total_n_points - i * constant_chunk_size.value()
                                           : constant_chunk_size.value());
        m_decompressed_chunk_offsets.push_back(
            i == 0 ? 0 : m_decompressed_chunk_offsets[i - 1] + m_n_points_per_chunk[i - 1]);
      } else {
        LASPP_UNIMPLEMENTED();
      }
    }
  }

  explicit LAZChunkTable() = default;

  void write(std::iostream& ostream) const {
    ostream.write(reinterpret_cast<const char*>(this),
                  static_cast<int64_t>(sizeof(LAZChunkTableHeader)));
    OutStream encoder(ostream);
    IntegerEncoder<32> int_encoder;
    for (size_t i = 0; i < number_of_chunks; i++) {
      uint32_t previous_chunk_size = i == 0 ? 0u : m_compressed_chunk_size[i - 1];
      if (m_constant_chunk_size) {
        int_encoder.encode_int(
            encoder, static_cast<int32_t>(m_compressed_chunk_size[i] - previous_chunk_size));
      } else {
        LASPP_UNIMPLEMENTED();
      }
    }
  }

  size_t num_chunks() const { return number_of_chunks; }
  size_t chunk_offset(size_t i) const { return m_compressed_chunk_offsets.at(i); }
  size_t compressed_chunk_size(size_t i) const { return m_compressed_chunk_size.at(i); }

  void add_chunk(size_t num_points, uint32_t compressed_size) {
    if (m_constant_chunk_size && m_compressed_chunk_size.size() > 0) {
      if (m_n_points_per_chunk.back() != m_constant_chunk_size.value()) {
        m_constant_chunk_size = std::nullopt;
      }
    } else if (m_n_points_per_chunk.size() == 0 && !m_constant_chunk_size) {
      m_constant_chunk_size = num_points;
    }
    m_compressed_chunk_offsets.push_back(
        m_compressed_chunk_offsets.empty()
            ? 8
            : (m_compressed_chunk_offsets.back() + m_compressed_chunk_size.back()));
    m_compressed_chunk_size.push_back(compressed_size);
    m_decompressed_chunk_offsets.push_back(m_decompressed_chunk_offsets.empty()
                                               ? 0
                                               : m_decompressed_chunk_offsets.back() +
                                                     m_n_points_per_chunk.back());
    m_n_points_per_chunk.push_back(num_points);
    number_of_chunks++;
  }

  const std::vector<size_t>& points_per_chunk() const { return m_n_points_per_chunk; }
  const std::vector<size_t>& decompressed_chunk_offsets() const {
    return m_decompressed_chunk_offsets;
  }
  std::optional<uint32_t> constant_chunk_size() const { return m_constant_chunk_size; }

  friend std::ostream& operator<<(std::ostream& os, const LAZChunkTable& chunk_table) {
    os << static_cast<const LAZChunkTableHeader>(chunk_table) << std::endl;
    os << "Compressed chunk sizes: " << chunk_table.m_compressed_chunk_size << std::endl;
    os << "Constant chunk size: " << chunk_table.m_constant_chunk_size << std::endl;
    os << "Compressed chunk offsets: " << chunk_table.m_compressed_chunk_offsets << std::endl;
    os << "Decompressed chunk offsets: " << chunk_table.m_decompressed_chunk_offsets << std::endl;
    os << "# points by chunk: " << chunk_table.m_n_points_per_chunk << std::endl;
    return os;
  }
};

#pragma pack(pop)

}  // namespace laspp
