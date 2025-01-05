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

#include <optional>
#include <span>
#include <vector>

#include "las_header.hpp"
#include "las_point.hpp"
#include "laz/chunktable.hpp"
#include "laz/laz_reader.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

namespace laspp {

class LASReader {
  std::istream& m_input_stream;
  LASHeader m_header;
  std::optional<LAZReader> m_laz_reader;
  std::vector<LASVLRWithGlobalOffset> m_vlr_headers;
  std::vector<LASEVLRWithGlobalOffset> m_evlr_headers;

  static LASHeader read_header(std::istream& input_stream) {
    input_stream.seekg(0);
    return LASHeader(input_stream);
  }

  template <typename T>
  std::vector<T> read_record_headers(int64_t initial_offset, size_t n_records) {
    std::vector<T> record_headers;
    m_input_stream.seekg(initial_offset);
    for (unsigned int i = 0; i < n_records; ++i) {
      typename T::record_type record;
      LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(&record),
                                           static_cast<int64_t>(sizeof(LASVLR))));
      record_headers.emplace_back(record, m_input_stream.tellg());
      auto end_of_header_offset = std::ios_base::cur;
      if constexpr (std::is_same_v<typename T::record_type, LASVLR>) {
        if (record.is_laz_vlr()) {
          LAZSpecialVLRContent laz_vlr(m_input_stream);
          m_laz_reader.emplace(LAZReader(laz_vlr));
        }
      }
      m_input_stream.seekg(static_cast<int64_t>(record.record_length_after_header),
                           end_of_header_offset);
    }
    return record_headers;
  }

  std::vector<LASVLRWithGlobalOffset> read_vlr_headers() {
    return read_record_headers<LASVLRWithGlobalOffset>(m_header.VLR_offset(), m_header.VLR_count());
  }

  std::vector<LASEVLRWithGlobalOffset> read_evlr_headers() {
    return read_record_headers<LASEVLRWithGlobalOffset>(
        static_cast<int64_t>(m_header.EVLR_offset()), m_header.EVLR_count());
  }

 public:
  explicit LASReader(std::istream& input_stream)
      : m_input_stream(input_stream),
        m_header(read_header(m_input_stream)),
        m_vlr_headers(read_vlr_headers()),
        m_evlr_headers(read_evlr_headers()) {
    if (m_header.is_laz_compressed()) {
      LASPP_ASSERT(m_laz_reader.has_value(), "LASReader: LAZ point format without LAZ VLR");
      m_input_stream.seekg(header().offset_to_point_data());
      m_laz_reader->read_chunk_table(m_input_stream, header().num_points());
    }
  }

  const LASHeader& header() const { return m_header; }

  const std::vector<LASVLRWithGlobalOffset>& vlr_headers() const { return m_vlr_headers; }
  const std::vector<LASEVLRWithGlobalOffset>& evlr_headers() const { return m_evlr_headers; }

  std::vector<std::byte> read_point_data(const LASHeader& header) {
    std::vector<std::byte> point_data(header.point_data_record_length() * header.num_points());
    m_input_stream.seekg(header.offset_to_point_data());
    LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(point_data.data()),
                                         static_cast<int64_t>(point_data.size())));
    return point_data;
  }

  size_t num_points() const { return m_header.num_points(); }
  size_t num_chunks() const {
    if (m_laz_reader.has_value()) {
      return m_laz_reader->chunk_table().num_chunks();
    }
    return 1;
  }
  std::vector<size_t> points_per_chunk() const {
    if (m_laz_reader.has_value()) {
      return m_laz_reader->chunk_table().points_per_chunk();
    }
    return {m_header.num_points()};
  }

 private:
  template <typename PointType, typename T>
  void read_points(std::span<T> points) {
    LASPP_ASSERT_EQ(sizeof(PointType), m_header.point_data_record_length());
    for (size_t i = 0; i < points.size(); i++) {
      PointType las_point;
      LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(&las_point),
                                           static_cast<int64_t>(sizeof(PointType))));
      if constexpr (is_copy_assignable<T, LASPointFormat0>() &&
                    std::is_base_of_v<LASPointFormat0, PointType>) {
        points[i] = static_cast<LASPointFormat0&>(las_point);
      } else if constexpr (std::is_base_of_v<LASPointFormat0, T> &&
                           std::is_base_of_v<LASPointFormat0, PointType>) {
        static_cast<LASPointFormat0&>(points[i]) = static_cast<LASPointFormat0&>(las_point);
      }
      if constexpr (std::is_base_of_v<GPSTime, PointType> && is_copy_assignable<T, GPSTime>()) {
        points[i] = static_cast<GPSTime&>(las_point);
      } else if constexpr (std::is_base_of_v<GPSTime, T> && std::is_base_of_v<GPSTime, PointType>) {
        static_cast<GPSTime&>(points[i]) = static_cast<GPSTime&>(las_point);
      }
      m_input_stream.seekg(
          static_cast<int64_t>(header().point_data_record_length() - sizeof(PointType)),
          std::ios_base::cur);
    }
  }

 public:
  template <typename T>
  std::span<T> read_chunk(std::span<T> output_location, size_t chunk_index) {
    if (header().is_laz_compressed()) {
      size_t start_offset = m_laz_reader->chunk_table().chunk_offset(chunk_index);
      m_input_stream.seekg(static_cast<int64_t>(header().offset_to_point_data() + start_offset));
      std::vector<std::byte> compressed_data(
          m_laz_reader->chunk_table().compressed_chunk_size(chunk_index));
      LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(compressed_data.data()),
                                           static_cast<int64_t>(compressed_data.size())));
      size_t n_points = m_laz_reader->chunk_table().points_per_chunk()[chunk_index];
      return m_laz_reader->decompress_chunk(compressed_data, output_location.subspan(0, n_points));
    }
    LASPP_ASSERT(chunk_index == 0);
    size_t n_points = num_points();
    m_input_stream.seekg(header().offset_to_point_data());
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      read_points<T>(output_location);
    } else {
      LASPP_SWITCH_OVER_POINT_TYPE(header().point_format(), read_points, output_location);
    }
    return output_location.subspan(0, n_points);
  }

  template <typename T>
  std::span<T> read_chunks(std::span<T> output_location, std::pair<size_t, size_t> chunk_indexes) {
    if (header().is_laz_compressed()) {
      size_t compressed_start_offset =
          m_laz_reader->chunk_table().chunk_offset(chunk_indexes.first);
      size_t total_compressed_size =
          m_laz_reader->chunk_table().compressed_chunk_size(chunk_indexes.second - 1) +
          m_laz_reader->chunk_table().chunk_offset(chunk_indexes.second - 1) -
          compressed_start_offset;
      size_t total_n_points =
          (chunk_indexes.second == m_laz_reader->chunk_table().num_chunks()
               ? header().num_points()
               : m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.second]) -
          m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.first];
      LASPP_ASSERT_GE(output_location.size(), total_n_points);
      std::vector<std::byte> compressed_data(total_compressed_size);
      m_input_stream.seekg(
          static_cast<int64_t>(header().offset_to_point_data() + compressed_start_offset));
      std::cout << "Reading " << total_compressed_size << " bytes from " << compressed_start_offset
                << " to " << compressed_start_offset + total_compressed_size << std::endl;
      LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(compressed_data.data()),
                                           static_cast<int64_t>(compressed_data.size())));

#pragma omp parallel for schedule(dynamic)
      for (size_t chunk_index = chunk_indexes.first; chunk_index < chunk_indexes.second;
           chunk_index++) {
        size_t start_offset =
            m_laz_reader->chunk_table().chunk_offset(chunk_index) - compressed_start_offset;
        size_t compressed_chunk_size =
            m_laz_reader->chunk_table().compressed_chunk_size(chunk_index);
        std::span<std::byte> compressed_chunk =
            std::span<std::byte>(compressed_data).subspan(start_offset, compressed_chunk_size);

        size_t point_offset =
            m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_index] -
            m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.first];
        size_t n_points = m_laz_reader->chunk_table().points_per_chunk()[chunk_index];
        if (chunk_index == 0) {
          LASPP_ASSERT_EQ(point_offset, 0u);
        }
        m_laz_reader->decompress_chunk(compressed_chunk,
                                       output_location.subspan(point_offset, n_points));
      }
      return output_location.subspan(0, total_n_points);
    }
    LASPP_ASSERT(chunk_indexes.first == 0);
    LASPP_ASSERT(chunk_indexes.second == 1);
    return read_chunk(output_location, 0);
  }

  std::vector<std::byte> read_vlr_data(const LASVLRWithGlobalOffset& vlr) {
    std::vector<std::byte> data(vlr.record_length_after_header);
    std::cout << "Reading " << vlr.record_length_after_header << " bytes from offset "
              << vlr.global_offset() << " " << m_input_stream.tellg() << std::endl;
    m_input_stream.seekg(static_cast<int64_t>(vlr.global_offset()));
    LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(data.data()),
                                         static_cast<int64_t>(data.size())));
    return data;
  }
};

}  // namespace laspp
