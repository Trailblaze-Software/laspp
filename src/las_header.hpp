
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

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

#include "las_point.hpp"
#include "utilities/assert.hpp"
#include "utilities/macros.hpp"

namespace laspp {

class LASWriter;

enum GlobalEncoding : uint16_t {
  GPS_TIME = 2 << 0,
  WAVEFORM_DATA_INTERNAL = 2 << 1,
  WAVEFORM_DATA_EXTERNAL = 2 << 2,
  SYNTHETIC_RETURN_NUMBERS = 2 << 3,
  WKT = 2 << 4,
};

inline std::string global_encoding_string(const uint16_t& encoding) {
  std::stringstream ss;
  if (encoding & GPS_TIME) ss << "GPS time, ";
  if (encoding & WAVEFORM_DATA_INTERNAL) ss << "Waveform data internal, ";
  if (encoding & WAVEFORM_DATA_EXTERNAL) ss << "Waveform data external, ";
  if (encoding & SYNTHETIC_RETURN_NUMBERS) ss << "Synthetic return numbers, ";
  if (encoding & WKT) ss << "WKT, ";
  return ss.str();
}

class Vector3D {
  std::array<double, 3> m_data;

 public:
  double& x() { return m_data[0]; }
  double& y() { return m_data[1]; }
  double& z() { return m_data[2]; }

  explicit Vector3D(std::istream& in_stream) {
    LASPP_CHECK_READ(in_stream.read(reinterpret_cast<char*>(m_data.data()), sizeof(m_data)));
  }

  Vector3D() = default;

  friend std::ostream& operator<<(std::ostream& os, const Vector3D& vec) {
    os << "(" << vec.m_data[0] << ", " << vec.m_data[1] << ", " << vec.m_data[2] << ")";
    return os;
  }
};

struct Transform {
  Vector3D m_scale_factors;
  Vector3D m_offsets;

  explicit Transform(std::istream& in_stream) : m_scale_factors(in_stream), m_offsets(in_stream) {}

  Transform() = default;

  friend std::ostream& operator<<(std::ostream& os, const Transform& transform) {
    os << "Scale factors: " << transform.m_scale_factors << std::endl;
    os << "Offsets: " << transform.m_offsets << std::endl;
    return os;
  }
};

template <size_t N>
void string_to_arr(const std::string& str, char (&arr)[N]) {
  for (size_t i = 0; i < N; ++i) {
    arr[i] = i < str.size() ? str[i] : '\0';
  }
}

template <typename T, size_t N>
std::string arr_to_string(const T (&arr)[N]) {
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < N; ++i) {
    ss << arr[i];
    if (i < N - 1) ss << ", ";
  }
  ss << "]";
  return ss.str();
}

#pragma pack(push, 1)
struct LASPP_PACKED LASHeaderPacked {
  char m_file_signature[4];
  uint16_t m_file_source_id;
  uint16_t m_global_encoding;
  uint32_t m_project_id_1;
  uint16_t m_project_id_2;
  uint16_t m_project_id_3;
  uint8_t m_project_id_4[8];
  uint8_t m_version_major;
  uint8_t m_version_minor;
  char m_system_id[32];
  char m_generating_software[32];
  uint16_t m_file_creation_day;
  uint16_t m_file_creation_year;
  uint16_t m_header_size;
  uint32_t m_offset_to_point_data;
  uint32_t m_number_of_variable_length_records;
  uint8_t m_point_data_record_format;
  uint16_t m_point_data_record_length;
  uint32_t m_legacy_number_of_point_records;
  uint32_t m_legacy_number_of_points_by_return[5];
  double m_scale_x;
  double m_scale_y;
  double m_scale_z;
  double m_offset_x;
  double m_offset_y;
  double m_offset_z;
  double m_max_x;
  double m_min_x;
  double m_max_y;
  double m_min_y;
  double m_max_z;
  double m_min_z;
  uint64_t m_start_of_waveform_data_packet_record;
  uint64_t m_start_of_first_extended_variable_length_record;
  uint32_t m_number_of_extended_variable_length_records;
  uint64_t m_number_of_point_records;
  uint64_t m_number_of_points_by_return[15];
};
#pragma pack(pop)

static_assert(sizeof(LASHeaderPacked) == 375);

class LASHeader {
  char m_file_signature[4] = {'L', 'A', 'S', 'F'};
  uint16_t m_file_source_id = 0;
  uint16_t m_global_encoding = 0;
  uint32_t m_project_id_1 = 0;
  uint16_t m_project_id_2 = 0;
  uint16_t m_project_id_3 = 0;
  uint8_t m_project_id_4[8] = {0};
  uint8_t m_version_major = 1;
  uint8_t m_version_minor = 4;
  char m_system_id[32] = {'\0'};
  char m_generating_software[32] = {'\0'};
  uint16_t m_file_creation_day = 0;
  uint16_t m_file_creation_year = 0;
  uint16_t m_header_size = sizeof(LASHeaderPacked);
  uint32_t m_offset_to_point_data = 0;
  uint32_t m_number_of_variable_length_records = 0;
  uint8_t m_point_data_record_format = 127;
  uint16_t m_point_data_record_length = 0;
  uint32_t m_legacy_number_of_point_records = 0;
  uint32_t m_legacy_number_of_points_by_return[5] = {0};
  Transform m_transform;
  double m_max_x = std::numeric_limits<double>::lowest();
  double m_min_x =
      std::numeric_limits<double>::max();  // Spec seems to disagree with this ordering :(
  double m_max_y = std::numeric_limits<double>::lowest();
  double m_min_y = std::numeric_limits<double>::max();
  double m_max_z = std::numeric_limits<double>::lowest();
  double m_min_z = std::numeric_limits<double>::max();
  uint64_t m_start_of_waveform_data_packet_record = 0;
  uint64_t m_start_of_first_extended_variable_length_record = 0;
  uint32_t m_number_of_extended_variable_length_records = 0;
  uint64_t m_number_of_point_records = 0;
  uint64_t m_number_of_points_by_return[15] = {0};

  template <typename F>
  void apply_all_in_order(F f) {
    f(m_file_signature);
    f(m_file_source_id);
    f(m_global_encoding);
    f(m_project_id_1);
    f(m_project_id_2);
    f(m_project_id_3);
    f(m_project_id_4);
    f(m_version_major);
    f(m_version_minor);
    f(m_system_id);
    f(m_generating_software);
    f(m_file_creation_day);
    f(m_file_creation_year);
    f(m_header_size);
    f(m_offset_to_point_data);
    f(m_number_of_variable_length_records);
    f(m_point_data_record_format);
    f(m_point_data_record_length);
    f(m_legacy_number_of_point_records);
    f(m_legacy_number_of_points_by_return);
    f(m_transform);
    f(m_max_x);
    f(m_min_x);
    f(m_max_y);
    f(m_min_y);
    f(m_max_z);
    f(m_min_z);
    f(m_start_of_waveform_data_packet_record);
    f(m_start_of_first_extended_variable_length_record);
    f(m_number_of_extended_variable_length_records);
    f(m_number_of_point_records);
    f(m_number_of_points_by_return);
  }

 public:
  explicit LASHeader(std::istream& in_stream) {
    apply_all_in_order([&](auto& val) {
      LASPP_CHECK_READ(in_stream.read(reinterpret_cast<char*>(&val), sizeof(val)));
    });
  }

  void write(std::ostream& out_stream) const {
    const_cast<LASHeader*>(this)->apply_all_in_order(
        [&](auto& val) { out_stream.write(reinterpret_cast<const char*>(&val), sizeof(val)); });
  }

  LASHeader() {
    string_to_arr("LAS++ by Trailblaze Software", m_generating_software);

    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);
    auto year_month_day = std::chrono::year_month_day{today};
    m_file_creation_year = static_cast<uint16_t>(static_cast<int>(year_month_day.year()));
    auto start_of_year =
        std::chrono::year_month_day{year_month_day.year() / std::chrono::January / 1};
    m_file_creation_day =
        static_cast<uint16_t>((today - std::chrono::sys_days{start_of_year}).count() + 1);
  }

  bool is_laz_compressed() const {
    return m_point_data_record_format &
           128;  // The spec says laz adds 100 but other implementations use 128
  }

  size_t num_points() const {
    return m_legacy_number_of_point_records == 0 ? m_number_of_point_records
                                                 : m_legacy_number_of_point_records;
  }

  std::array<double, 3> transform(std::array<int32_t, 3> pos) {
    return {
        m_transform.m_scale_factors.x() * pos[0] + m_transform.m_offsets.x(),
        m_transform.m_scale_factors.y() * pos[1] + m_transform.m_offsets.y(),
        m_transform.m_scale_factors.z() * pos[2] + m_transform.m_offsets.z(),
    };
  }

  void update_bounds(std::array<int32_t, 3> pos) {
    std::array<double, 3> transformed = transform(pos);
    m_max_x = std::max(m_max_x, transformed[0]);
    m_min_x = std::min(m_min_x, transformed[0]);
    m_max_y = std::max(m_max_y, transformed[1]);
    m_min_y = std::min(m_min_y, transformed[1]);
    m_max_z = std::max(m_max_z, transformed[2]);
    m_min_z = std::min(m_min_z, transformed[2]);
  }

  void set_point_format(uint8_t point_format, uint16_t num_extra_bytes) {
    m_point_data_record_format = point_format;
    m_point_data_record_length = size_of_point_format(point_format) + num_extra_bytes;
  }

  size_t size() const { return m_header_size; }

  unsigned int point_data_record_length() const { return m_point_data_record_length; }

  unsigned int offset_to_point_data() const { return m_offset_to_point_data; }

  unsigned int VLR_offset() const { return m_header_size; }
  unsigned int VLR_count() const { return m_number_of_variable_length_records; }

  size_t EVLR_offset() const {
    return offsetof(LASHeader, m_start_of_first_extended_variable_length_record) < m_header_size
               ? m_start_of_first_extended_variable_length_record
               : 0;
  }
  size_t EVLR_count() const {
    return offsetof(LASHeader, m_start_of_first_extended_variable_length_record) < m_header_size
               ? m_number_of_extended_variable_length_records
               : 0;
  }

  Transform& transform() { return m_transform; }
  const Transform& transform() const { return m_transform; }

  uint8_t point_format() const { return m_point_data_record_format; }
  int num_extra_bytes() const {
    return m_point_data_record_length - size_of_point_format(m_point_data_record_format);
  }

  friend std::ostream& operator<<(std::ostream& os, const LASHeader& header) {
    os << "File signature: " << header.m_file_signature[0] << header.m_file_signature[1]
       << header.m_file_signature[2] << header.m_file_signature[3] << std::endl;
    os << "File source ID: " << header.m_file_source_id << std::endl;
    os << "Global encoding: " << global_encoding_string(header.m_global_encoding) << std::endl;
    os << "Project ID 1: " << header.m_project_id_1 << std::endl;
    os << "Project ID 2: " << header.m_project_id_2 << std::endl;
    os << "Project ID 3: " << header.m_project_id_3 << std::endl;
    os << "Project ID 4: " << arr_to_string(header.m_project_id_4) << std::endl;
    os << "Version major: " << static_cast<int>(header.m_version_major) << std::endl;
    os << "Version minor: " << static_cast<int>(header.m_version_minor) << std::endl;
    os << "System ID: " << header.m_system_id << std::endl;
    os << "Generating software: " << header.m_generating_software << std::endl;
    os << "File creation day: " << header.m_file_creation_day << std::endl;
    os << "File creation year: " << header.m_file_creation_year << std::endl;
    os << "Header size: " << header.m_header_size << std::endl;
    os << "Offset to point data: " << header.m_offset_to_point_data << std::endl;
    os << "Number of variable length records: " << header.m_number_of_variable_length_records
       << std::endl;
    os << "Point data record format: " << static_cast<int>(header.m_point_data_record_format)
       << std::endl;
    os << "Point data record length: " << header.m_point_data_record_length << std::endl;
    os << "Legacy number of point records: " << header.m_legacy_number_of_point_records
       << std::endl;
    os << "Legacy number of points by return: "
       << arr_to_string(header.m_legacy_number_of_points_by_return) << std::endl;
    os << header.m_transform;
    os << "Max X: " << header.m_max_x << std::endl;
    os << "Max Y: " << header.m_max_y << std::endl;
    os << "Max Z: " << header.m_max_z << std::endl;
    os << "Min X: " << header.m_min_x << std::endl;
    os << "Min Y: " << header.m_min_y << std::endl;
    os << "Min Z: " << header.m_min_z << std::endl;
    os << "Start of waveform data packet record: " << header.m_start_of_waveform_data_packet_record
       << std::endl;
    os << "Start of first extended variable length record: "
       << header.m_start_of_first_extended_variable_length_record << std::endl;
    os << "Number of extended variable length records: "
       << header.m_number_of_extended_variable_length_records << std::endl;
    os << "Number of point records: " << header.m_number_of_point_records << std::endl;
    os << "Number of points by return: " << arr_to_string(header.m_number_of_points_by_return)
       << std::endl;
    return os;
  }

  friend LASWriter;
};

}  // namespace laspp
