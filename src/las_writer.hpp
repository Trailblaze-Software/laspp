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
#include <type_traits>
#include <vector>

#include "las_header.hpp"
#include "las_point.hpp"
#include "laz/laz_writer.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

namespace laspp {

enum class WritingStage { VLRS, POINTS, CHUNKTABLE, EVLRS, HEADER };

std::ostream& operator<<(std::ostream& os, WritingStage stage) {
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
  bool m_written_chunktable = false;

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

 private:
  template <typename PointType, typename T>
  void t_write_points(const std::span<T>& points) {
    LASPP_ASSERT_EQ(sizeof(PointType), m_header.point_data_record_length());
    LASPP_ASSERT_LE(m_stage, WritingStage::POINTS);
    m_stage = WritingStage::POINTS;
    LASPP_ASSERT_EQ(m_header.offset_to_point_data(), m_output_stream.tellp());
    m_output_stream.seekp(m_header.offset_to_point_data());

    std::vector<PointType> points_to_write(points.size());

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
        if constexpr (std::is_base_of_v<LASPointFormat0, PointType> &&
                      is_copy_assignable<LASPointFormat0, T>()) {
          static_cast<LASPointFormat0&>(points_to_write[i]) =
              static_cast<LASPointFormat0>(points[i]);
          if (points_to_write[i].bit_byte.return_number < 15)
            local_points_by_return[points_to_write[i].bit_byte.return_number]++;
          local_min_pos[0] = std::min(local_min_pos[0], points_to_write[i].x);
          local_min_pos[1] = std::min(local_min_pos[1], points_to_write[i].y);
          local_min_pos[2] = std::min(local_min_pos[2], points_to_write[i].z);
          local_max_pos[0] = std::max(local_max_pos[0], points_to_write[i].x);
          local_max_pos[1] = std::max(local_max_pos[1], points_to_write[i].y);
          local_max_pos[2] = std::max(local_max_pos[2], points_to_write[i].z);
        }
        if constexpr (std::is_base_of_v<GPSTime, PointType> && is_copy_assignable<GPSTime, T>()) {
          static_cast<GPSTime&>(points_to_write[i]) = static_cast<GPSTime>(points[i]);
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
        header().point_format() < 6) {
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

    std::cout << "Writing " << points_to_write.size() << " points ("
              << sizeof(PointType) * points_to_write.size() << " B)" << std::endl;
    m_output_stream.write(reinterpret_cast<const char*>(points_to_write.data()),
                          static_cast<int64_t>(points_to_write.size() * sizeof(PointType)));
  }

 public:
  template <typename T>
  void write_points(const std::span<T>& points) {
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      t_write_points<T>(points);
    } else {
      LASPP_SWITCH_OVER_POINT_TYPE(header().point_format(), t_write_points, points)
    }
  }

 private:
  void write_chunktable() {
    if (header().is_laz_compressed() && !m_written_chunktable) {
      LASPP_ASSERT_LE(m_stage, WritingStage::CHUNKTABLE);
      m_stage = WritingStage::CHUNKTABLE;

      m_written_chunktable = true;
    }
  }

 public:
  void write_evlr(const LASEVLR& evlr, const std::vector<std::byte>& data) {
    write_chunktable();
    LASPP_ASSERT_LE(m_stage, WritingStage::EVLRS);
    m_stage = WritingStage::EVLRS;
    LASPP_ASSERT_EQ(evlr.record_length_after_header, data.size());
    m_output_stream.write(reinterpret_cast<const char*>(&evlr), sizeof(LASEVLR));
    m_output_stream.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<int64_t>(evlr.record_length_after_header));
    header().m_number_of_extended_variable_length_records++;
  }

  ~LASWriter() {
    write_chunktable();
    write_header();
  }
};

}  // namespace laspp
