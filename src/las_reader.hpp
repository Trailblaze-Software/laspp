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
#include <type_traits>
#include <vector>

#include "example_custom_las_point.hpp"
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
  std::optional<std::string> m_math_wkt;
  std::optional<std::string> m_coordinate_wkt;
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
                                           static_cast<int64_t>(sizeof(typename T::record_type))));
      record_headers.emplace_back(record, m_input_stream.tellg());
      int64_t end_of_header_offset = m_input_stream.tellg();
      if constexpr (std::is_same_v<typename T::record_type, LASVLR>) {
        if (record.is_laz_vlr()) {
          LAZSpecialVLRContent laz_vlr(m_input_stream);
          m_laz_reader.emplace(LAZReader(laz_vlr));
        }
        if (record.is_projection()) {
          std::cout << record << std::endl;
          if (record.is_ogc_math_transform_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(m_input_stream.read(wkt.data(), record.record_length_after_header));
            std::string wkt_string(wkt.begin(), wkt.end());
            m_math_wkt.emplace(wkt_string);
          }
          if (record.is_ogc_coordinate_system_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(m_input_stream.read(wkt.data(), record.record_length_after_header));
            std::string wkt_string(wkt.begin(), wkt.end());
            m_coordinate_wkt.emplace(wkt_string);
          }
          if (record.is_geo_key_directory()) {
            GeoKeys keys(m_input_stream);
            LASPP_ASSERT_EQ(m_input_stream.tellg(),
                            end_of_header_offset + record.record_length_after_header);
          }
          if (record.is_geo_double_params()) {
            LASPP_ASSERT_EQ(record.record_length_after_header % sizeof(double), 0);
            std::vector<double> params(record.record_length_after_header / sizeof(double));
            LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(params.data()),
                                                 record.record_length_after_header));
          }
          if (record.is_geo_ascii_params()) {
            std::vector<char> params(record.record_length_after_header);
            LASPP_CHECK_READ(m_input_stream.read(params.data(), record.record_length_after_header));
          }
        }
      }
      m_input_stream.seekg(static_cast<int64_t>(record.record_length_after_header) +
                           end_of_header_offset);
      LASPP_ASSERT_NE(m_input_stream.tellg(), -1);
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
  template <typename CopyType, typename PointType, typename T>
  static void copy_if_possible(const PointType& las_point, T& point) {
    if constexpr (std::is_base_of_v<CopyType, PointType>) {
      if constexpr (is_copy_fromable<T, CopyType>()) {
        copy_from(point, static_cast<const CopyType&>(las_point));
      } else if constexpr (is_copy_assignable<T, CopyType>()) {
        point = static_cast<const CopyType&>(las_point);
      } else if constexpr (std::is_base_of_v<CopyType, T>) {
        static_cast<CopyType&>(point) = static_cast<const CopyType&>(las_point);
      }
    }
  }

  template <typename PointType, typename T>
  void read_points(std::span<T> points) {
    LASPP_ASSERT_EQ(sizeof(PointType), m_header.point_data_record_length());
    for (size_t i = 0; i < points.size(); i++) {
      PointType las_point;
      LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(&las_point),
                                           static_cast<int64_t>(sizeof(PointType))));
      static_assert(is_copy_assignable<ExampleMinimalLASPoint, LASPointFormat0>());
      static_assert(is_copy_assignable<ExampleFullLASPoint, LASPointFormat0>());
      static_assert(is_copy_fromable<ExampleFullLASPoint, GPSTime>());
      static_assert(is_convertable<T, LASPointFormat0>() || is_convertable<T, GPSTime>(),
                    "PointType should use data from LAS file");
      copy_if_possible<LASPointFormat0>(las_point, points[i]);
      copy_if_possible<GPSTime>(las_point, points[i]);
      m_input_stream.seekg(
          static_cast<int64_t>(header().point_data_record_length() - sizeof(PointType)),
          std::ios_base::cur);
    }
  }

 public:
  std::optional<std::string> math_wkt() const { return m_math_wkt; }
  std::optional<std::string> coordinate_wkt() const { return m_coordinate_wkt; }
  std::optional<std::string> wkt() const {
    return m_coordinate_wkt.has_value() ? m_coordinate_wkt : m_math_wkt;
  }

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
    m_input_stream.seekg(static_cast<int64_t>(vlr.global_offset()));
    LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(data.data()),
                                         static_cast<int64_t>(data.size())));
    return data;
  }

  std::vector<std::byte> read_evlr_data(const LASEVLRWithGlobalOffset& evlr) {
    std::vector<std::byte> data(evlr.record_length_after_header);
    m_input_stream.seekg(static_cast<int64_t>(evlr.global_offset()));
    LASPP_CHECK_READ(m_input_stream.read(reinterpret_cast<char*>(data.data()),
                                         static_cast<int64_t>(data.size())));
    return data;
  }
};

}  // namespace laspp
