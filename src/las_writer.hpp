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
#include <span>
#include <sstream>
#include <type_traits>
#include <vector>

#include "example_custom_las_point.hpp"
#include "las_header.hpp"
#include "las_point.hpp"
#include "las_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "laz/laz_writer.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

namespace laspp {

enum class WritingStage { VLRS, POINTS, CHUNKTABLE, EVLRS, HEADER };

inline std::ostream& operator<<(std::ostream& os, WritingStage stage) {
  switch (stage) {
    case WritingStage::VLRS:
      os << "WritingStage::VLRS";
      break;
    case WritingStage::POINTS:
      os << "WritingStage::POINTS";
      break;
    case WritingStage::CHUNKTABLE:
      os << "WritingStage::CHUNKTABLE";
      break;
    case WritingStage::EVLRS:
      os << "WritingStage::EVLRS";
      break;
    case WritingStage::HEADER:
      os << "WritingStage::HEADER";
      break;
  }
  return os;
}

class LASWriter {
  std::iostream& m_output_stream;

  LASHeader m_header;

  WritingStage m_stage = WritingStage::VLRS;
  std::optional<LAZWriter> m_laz_writer;
  bool m_written_chunktable = false;
  int64_t m_laz_vlr_offset = -1;

  void write_header() {
    m_output_stream.seekp(0);
    m_header.write(m_output_stream);
  }

 public:
  explicit LASWriter(std::iostream& ofs, uint8_t point_format, uint16_t num_extra_bytes = 0)
      : m_output_stream(ofs) {
    LASPP_ASSERT_EQ(num_extra_bytes, 0);
    header().set_point_format(point_format, num_extra_bytes);
    header().m_offset_to_point_data = static_cast<uint32_t>(header().size());
    // placeholder
    header().write(m_output_stream);
  }

  const LASHeader& header() const { return m_header; }
  LASHeader& header() { return m_header; }

  // Copy header metadata from another header (preserves writer-managed fields)
  void copy_header_metadata(const LASHeader& source_header) {
    m_header.m_file_source_id = source_header.m_file_source_id;
    m_header.m_global_encoding = source_header.m_global_encoding;
    m_header.m_project_id_1 = source_header.m_project_id_1;
    m_header.m_project_id_2 = source_header.m_project_id_2;
    m_header.m_project_id_3 = source_header.m_project_id_3;
    std::memcpy(m_header.m_project_id_4, source_header.m_project_id_4,
                sizeof(m_header.m_project_id_4));
    std::memcpy(m_header.m_system_id, source_header.m_system_id, sizeof(m_header.m_system_id));
    m_header.transform() = source_header.transform();
  }

  void write_vlr(const LASVLR& vlr, std::span<const std::byte> data) {
    LASPP_ASSERT_EQ(m_stage, WritingStage::VLRS);
    LASPP_ASSERT_EQ(vlr.record_length_after_header, data.size());
    m_output_stream.write(reinterpret_cast<const char*>(&vlr), sizeof(LASVLR));
    m_output_stream.write(reinterpret_cast<const char*>(data.data()),
                          vlr.record_length_after_header);
    header().m_number_of_variable_length_records++;
    header().m_offset_to_point_data +=
        static_cast<uint32_t>(sizeof(LASVLR) + vlr.record_length_after_header);
  }

  void write_wkt(const std::string& wkt, bool math_transform_wkt = false) {
    LASVLR wkt_vlr;
    wkt_vlr.reserved = 0;
    string_to_arr("LASF_Projection", wkt_vlr.user_id);
    wkt_vlr.record_id = math_transform_wkt ? 2111 : 2112;
    wkt_vlr.record_length_after_header = static_cast<uint16_t>(wkt.size() + 1);
    string_to_arr("OGC WKT", wkt_vlr.description);
    write_vlr(wkt_vlr, std::span(reinterpret_cast<const std::byte*>(wkt.c_str()), wkt.size() + 1));
  }

 private:
  template <typename CopyType, typename PointType, typename T>
  static void copy_if_possible(PointType& dest, const T& src) {
    if constexpr (std::is_base_of_v<CopyType, PointType>) {
      if constexpr (is_copy_fromable<CopyType&, T>()) {
        copy_from(static_cast<CopyType&>(dest), src);
      } else if constexpr (is_copy_assignable<CopyType, T>()) {
        static_cast<CopyType&>(dest) = static_cast<CopyType>(src);
      }
    }
  }

  template <typename PointType, typename T>
  void t_write_points(const std::span<const T>& points, std::optional<size_t> chunk_size) {
    LASPP_ASSERT_EQ(sizeof(PointType), m_header.point_data_record_length());
    LASPP_ASSERT_LE(m_stage, WritingStage::POINTS);
    if (m_header.is_laz_compressed()) {
      if (m_stage < WritingStage::POINTS) {
        LAZSpecialVLRContent laz_vlr_content(std::is_base_of_v<LASPointFormat6, PointType>
                                                 ? LAZCompressor::LayeredChunked
                                                 : LAZCompressor::PointwiseChunked);

        if constexpr (std::is_base_of_v<LASPointFormat0, PointType>) {
          laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::Point10));
        }
        if constexpr (std::is_base_of_v<LASPointFormat6, PointType>) {
          laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::Point14));
        }
        if constexpr (std::is_base_of_v<GPSTime, PointType>) {
          laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::GPSTime11));
        }
        if constexpr (std::is_base_of_v<ColorData, PointType>) {
          if constexpr (std::is_base_of_v<LASPointFormat0, PointType>) {
            laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::RGB12));
          } else if constexpr (std::is_base_of_v<LASPointFormat6, PointType>) {
            laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::RGB14));
          } else {
            static_assert(!std::is_base_of_v<ColorData, PointType>,
                          "ColorData is only supported alongside point format 0 and 6");
          }
        }
        if constexpr (std::is_base_of_v<WavePacketData, PointType>) {
          laz_vlr_content.add_item_record(LAZItemRecord(LAZItemType::Wavepacket13));
        }
        std::stringstream laz_vlr_content_stream;
        laz_vlr_content.write_to(laz_vlr_content_stream);
        std::vector<char> laz_vlr_content_char(
            (std::istreambuf_iterator<char>(laz_vlr_content_stream)),
            std::istreambuf_iterator<char>());
        std::vector<std::byte> laz_vlr_content_bytes;
        laz_vlr_content_bytes.reserve(laz_vlr_content_char.size());
        for (char c : laz_vlr_content_char) {
          laz_vlr_content_bytes.push_back(static_cast<std::byte>(c));
        }

        LASVLR laz_vlr;
        laz_vlr.reserved = 0xAABB;
        string_to_arr("laszip encoded", laz_vlr.user_id);
        laz_vlr.record_id = 22204;
        laz_vlr.record_length_after_header = static_cast<uint16_t>(laz_vlr_content_bytes.size());
        string_to_arr("LAZ VLR", laz_vlr.description);

        m_laz_vlr_offset = m_output_stream.tellp();
        write_vlr(laz_vlr, laz_vlr_content_bytes);

        LASPP_ASSERT_EQ(m_header.offset_to_point_data(), m_output_stream.tellp());

        m_laz_writer.emplace(m_output_stream, std::move(laz_vlr_content));
      } else {
        LASPP_ASSERT(m_laz_writer.has_value());
      }
    }
    m_stage = WritingStage::POINTS;

    std::vector<PointType> points_to_write(points.size());
    memset(points_to_write.data(), 0, points_to_write.size() * sizeof(PointType));

    size_t points_by_return[15] = {0};
    int32_t min_pos[3] = {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(),
                          std::numeric_limits<int32_t>::max()};
    int32_t max_pos[3] = {std::numeric_limits<int32_t>::lowest(),
                          std::numeric_limits<int32_t>::lowest(),
                          std::numeric_limits<int32_t>::lowest()};
#pragma omp parallel
    {
      size_t local_points_by_return[15] = {0};
      int32_t local_min_pos[3] = {std::numeric_limits<int32_t>::max(),
                                  std::numeric_limits<int32_t>::max(),
                                  std::numeric_limits<int32_t>::max()};
      int32_t local_max_pos[3] = {std::numeric_limits<int32_t>::lowest(),
                                  std::numeric_limits<int32_t>::lowest(),
                                  std::numeric_limits<int32_t>::lowest()};
#pragma omp for
      for (size_t i = 0; i < points.size(); i++) {
        static_assert(is_copy_assignable<LASPointFormat0, ExampleFullLASPoint>());
        static_assert(is_copy_fromable<GPSTime, ExampleFullLASPoint>());

        copy_if_possible<LASPointFormat0>(points_to_write[i], points[i]);
        copy_if_possible<LASPointFormat6>(points_to_write[i], points[i]);
        copy_if_possible<GPSTime>(points_to_write[i], points[i]);
        copy_if_possible<ColorData>(points_to_write[i], points[i]);
        copy_if_possible<WavePacketData>(points_to_write[i], points[i]);
        if constexpr (std::is_base_of_v<LASPointFormat0, PointType>) {
          if (points_to_write[i].bit_byte.return_number < 16 &&
              points_to_write[i].bit_byte.return_number > 0) {
            local_points_by_return[points_to_write[i].bit_byte.return_number - 1]++;
          }
          PointType point = points_to_write[i];
          local_min_pos[0] = std::min(local_min_pos[0], point.x);
          local_min_pos[1] = std::min(local_min_pos[1], point.y);
          local_min_pos[2] = std::min(local_min_pos[2], point.z);
          local_max_pos[0] = std::max(local_max_pos[0], point.x);
          local_max_pos[1] = std::max(local_max_pos[1], point.y);
          local_max_pos[2] = std::max(local_max_pos[2], point.z);
        }
      }
#pragma omp critical
      {
        for (int i = 0; i < 15; i++) {
          points_by_return[i] += local_points_by_return[i];
        }
        min_pos[0] = std::min(min_pos[0], local_min_pos[0]);
        min_pos[1] = std::min(min_pos[1], local_min_pos[1]);
        min_pos[2] = std::min(min_pos[2], local_min_pos[2]);
        max_pos[0] = std::max(max_pos[0], local_max_pos[0]);
        max_pos[1] = std::max(max_pos[1], local_max_pos[1]);
        max_pos[2] = std::max(max_pos[2], local_max_pos[2]);
      }
    }

    header().m_number_of_point_records += points.size();
    for (int i = 0; i < 15; i++) {
      header().m_number_of_points_by_return[i] += points_by_return[i];
    }
    if (header().m_number_of_point_records < std::numeric_limits<uint32_t>::max() &&
        (header().point_format() < 6 ||
         (header().point_format() >= 128 && header().point_format() < 128 + 6))) {
      header().m_legacy_number_of_point_records =
          static_cast<uint32_t>(header().m_number_of_point_records);
      for (int i = 0; i < 5; i++) {
        header().m_legacy_number_of_points_by_return[i] =
            static_cast<uint32_t>(header().m_number_of_points_by_return[i]);
      }
    } else {
      header().m_legacy_number_of_point_records = 0;
      for (int i = 0; i < 5; i++) {
        header().m_legacy_number_of_points_by_return[i] = 0;
      }
    }

    header().update_bounds({min_pos[0], min_pos[1], min_pos[2]});
    header().update_bounds({max_pos[0], max_pos[1], max_pos[2]});

    if (m_header.is_laz_compressed()) {
      if (chunk_size.has_value()) {
        std::vector<std::span<PointType>> chunks;
        chunks.reserve(points.size() / chunk_size.value() + 1);
        for (size_t i = 0; i < points.size(); i += chunk_size.value()) {
          size_t num_points = std::min(chunk_size.value(), points.size() - i);
          chunks.push_back(std::span<PointType>(points_to_write).subspan(i, num_points));
        }
        m_laz_writer->write_chunks(std::span<std::span<PointType>>(chunks));
      } else {
        m_laz_writer->write_chunk(std::span<PointType>(points_to_write));
      }
    } else {
      m_output_stream.write(reinterpret_cast<const char*>(points_to_write.data()),
                            static_cast<int64_t>(points_to_write.size() * sizeof(PointType)));
    }
  }

 public:
  template <typename T>
  void write_points(const std::span<const T>& points,
                    std::optional<size_t> chunk_size = std::nullopt) {
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      t_write_points<T>(points, chunk_size);
    } else {
      LASPP_SWITCH_OVER_POINT_TYPE(header().point_format(), t_write_points, points, chunk_size);
    }
  }

  // Write raw point data (for cases where you have raw bytes, e.g., with extra bytes)
  // This updates the point count and offset automatically
  void write_raw_point_data(std::span<const std::byte> raw_point_data, size_t num_points) {
    LASPP_ASSERT_LE(m_stage, WritingStage::POINTS);
    LASPP_ASSERT(!m_header.is_laz_compressed(),
                 "Raw point data writing not supported for compressed files");
    LASPP_ASSERT_EQ(raw_point_data.size(), num_points * m_header.point_data_record_length(),
                    "Raw point data size doesn't match expected size");

    // Set stage to POINTS if not already there
    if (m_stage < WritingStage::POINTS) {
      m_stage = WritingStage::POINTS;
      // Update offset_to_point_data to current stream position
      header().m_offset_to_point_data = static_cast<uint32_t>(m_output_stream.tellp());
    }

    // Write raw point data
    m_output_stream.write(reinterpret_cast<const char*>(raw_point_data.data()),
                          static_cast<int64_t>(raw_point_data.size()));

    // Update point count (normally done automatically by write_points())
    header().m_number_of_point_records = num_points;
    if (header().m_number_of_point_records < std::numeric_limits<uint32_t>::max() &&
        (header().point_format() < 6 ||
         (header().point_format() >= 128 && header().point_format() < 128 + 6))) {
      header().m_legacy_number_of_point_records = static_cast<uint32_t>(num_points);
    } else {
      header().m_legacy_number_of_point_records = 0;
    }
  }

 private:
  void write_chunktable() {
    if (header().is_laz_compressed() && !m_written_chunktable) {
      LASPP_ASSERT_LE(m_stage, WritingStage::CHUNKTABLE);
      LASPP_ASSERT_GE(m_laz_vlr_offset, 0);
      m_stage = WritingStage::CHUNKTABLE;
      LAZSpecialVLRContent laz_vlr_content(m_laz_writer->special_vlr());
      m_laz_writer.reset();
      m_output_stream.seekp(m_laz_vlr_offset + static_cast<int64_t>(sizeof(LASVLR)));
      laz_vlr_content.write_to(m_output_stream);
      m_output_stream.seekp(0, std::ios::end);
      header().m_start_of_first_extended_variable_length_record =
          static_cast<size_t>(m_output_stream.tellp());
      m_written_chunktable = true;
    }
  }

 public:
  void write_evlr(const LASEVLR& evlr, const std::span<const std::byte>& data) {
    write_chunktable();
    LASPP_ASSERT_LE(m_stage, WritingStage::EVLRS);
    if (m_stage < WritingStage::EVLRS) {
      header().m_start_of_first_extended_variable_length_record =
          static_cast<size_t>(m_output_stream.tellp());
    }
    m_stage = WritingStage::EVLRS;
    LASPP_ASSERT_EQ(evlr.record_length_after_header, data.size());
    m_output_stream.write(reinterpret_cast<const char*>(&evlr), sizeof(LASEVLR));
    m_output_stream.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<int64_t>(evlr.record_length_after_header));
    header().m_number_of_extended_variable_length_records++;
  }

  void write_lastools_spatial_index(const QuadtreeSpatialIndex& spatial_index) {
    // Write spatial index to a buffer first to get the size
    std::stringstream index_stream;
    spatial_index.write(index_stream);
    std::vector<char> index_data((std::istreambuf_iterator<char>(index_stream)),
                                 std::istreambuf_iterator<char>());

    // Create EVLR header
    LASEVLR evlr;
    evlr.reserved = 0;
    string_to_arr("LAStools", evlr.user_id);
    evlr.record_id = 30;
    evlr.record_length_after_header = index_data.size();
    string_to_arr("LAX spatial indexing (LASindex)", evlr.description);

    write_evlr(evlr, std::span(reinterpret_cast<std::byte*>(index_data.data()), index_data.size()));
  }

 private:
  // Helper to convert int32 coordinates to double using scale/offset
  static inline double int32_to_double(int32_t coord, double scale, double offset) {
    return coord * scale + offset;
  }

  template <typename PointType>
  void copy_points_with_spatial_index(LASReader& reader, bool add_spatial_index) {
    std::vector<PointType> points(reader.num_points());
    reader.read_chunks<PointType>(points, {0, reader.num_chunks()});

    if (add_spatial_index) {
      // Reorder points by quadtree cell index (always done when adding spatial index)
      double scale_x = reader.header().transform().scale_factors().x();
      double offset_x = reader.header().transform().offsets().x();
      double scale_y = reader.header().transform().scale_factors().y();
      double offset_y = reader.header().transform().offsets().y();

      // Build initial spatial index to get cell indices
      // We need to build it from points first to get the quadtree structure
      QuadtreeSpatialIndex temp_index(reader.header(), points);

      // Create vector of (cell_index, original_index) pairs for sorting
      std::vector<std::pair<int32_t, size_t>> point_cell_pairs;
      point_cell_pairs.reserve(points.size());

      for (size_t i = 0; i < points.size(); ++i) {
        double x = int32_to_double(points[i].x, scale_x, offset_x);
        double y = int32_to_double(points[i].y, scale_y, offset_y);
        int32_t cell_index = temp_index.get_cell_index(x, y);
        point_cell_pairs.push_back({cell_index, i});
      }

      // Sort by cell index, then by original index
      std::sort(point_cell_pairs.begin(), point_cell_pairs.end());

      // Reorder points
      std::vector<PointType> reordered_points;
      reordered_points.reserve(points.size());
      for (const auto& pair : point_cell_pairs) {
        reordered_points.push_back(points[pair.second]);
      }
      points = std::move(reordered_points);

      // Build final spatial index with reordered points
      QuadtreeSpatialIndex spatial_index(reader.header(), points);
      std::cout << "Built spatial index with " << spatial_index.num_cells() << " cells for "
                << points.size() << " points." << std::endl;

      // Write points
      write_points<PointType>(points);

      // Write spatial index
      write_lastools_spatial_index(spatial_index);
    } else {
      // Regular write without spatial index
      write_points<PointType>(points);
    }
  }

 public:
  // Copy all data from a reader to this writer
  void copy_from_reader(LASReader& reader, bool add_spatial_index = false) {
    // Copy header metadata (preserves writer-managed fields)
    copy_header_metadata(reader.header());

    // Copy VLRs (skip LAZ VLR since compression is handled by point format)
    for (const auto& vlr : reader.vlr_headers()) {
      if (vlr.is_laz_vlr()) {
        continue;
      }
      write_vlr(vlr, reader.read_vlr_data(vlr));
    }

    // Read and write points (with optional spatial index)
    LASPP_SWITCH_OVER_POINT_TYPE(reader.header().point_format(), copy_points_with_spatial_index,
                                 reader, add_spatial_index);

    // Copy EVLRs (skip spatial index EVLR if we're adding a new one)
    for (const auto& evlr : reader.evlr_headers()) {
      if (add_spatial_index && evlr.is_lastools_spatial_index_evlr()) {
        continue;  // Skip existing spatial index, we'll write a new one
      }
      auto data = reader.read_evlr_data(evlr);
      write_evlr(evlr, data);
    }
  }

  ~LASWriter() {
    write_chunktable();
    write_header();
  }
};

}  // namespace laspp
