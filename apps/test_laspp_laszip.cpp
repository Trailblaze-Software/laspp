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

// Windows compatibility: prevent min/max macros and byte typedef conflicts
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <laszip_api.h>

// Undefine Windows byte typedef if it exists to avoid conflicts with std::byte
#ifdef _WIN32
#ifdef byte
#undef byte
#endif
#endif

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

namespace {

// Helper function to convert laszip_point to LASPointFormat0
LASPointFormat0 convert_laszip_to_format0(const laszip_point& laszip_pt) {
  LASPointFormat0 pt;
  pt.x = laszip_pt.X;
  pt.y = laszip_pt.Y;
  pt.z = laszip_pt.Z;
  pt.intensity = laszip_pt.intensity;
  pt.bit_byte.return_number = static_cast<uint8_t>(laszip_pt.return_number & 0x07);
  pt.bit_byte.number_of_returns = static_cast<uint8_t>(laszip_pt.number_of_returns & 0x07);
  pt.bit_byte.scan_direction_flag = static_cast<uint8_t>(laszip_pt.scan_direction_flag);
  pt.bit_byte.edge_of_flight_line = static_cast<uint8_t>(laszip_pt.edge_of_flight_line);
  pt.classification_byte.classification =
      static_cast<LASClassification>(laszip_pt.classification & 0x1F);
  pt.classification_byte.synthetic = static_cast<uint8_t>(laszip_pt.synthetic_flag);
  pt.classification_byte.key_point = static_cast<uint8_t>(laszip_pt.keypoint_flag);
  pt.classification_byte.withheld = static_cast<uint8_t>(laszip_pt.withheld_flag);
  pt.scan_angle_rank = static_cast<uint8_t>(static_cast<int8_t>(laszip_pt.scan_angle_rank));
  pt.user_data = laszip_pt.user_data;
  pt.point_source_id = laszip_pt.point_source_ID;
  return pt;
}

// Helper function to convert laszip_point to LASPointFormat1
LASPointFormat1 convert_laszip_to_format1(const laszip_point& laszip_pt) {
  LASPointFormat1 pt;
  static_cast<LASPointFormat0&>(pt) = convert_laszip_to_format0(laszip_pt);
  pt.gps_time.f64 = laszip_pt.gps_time;
  return pt;
}

// Helper function to convert laszip_point to LASPointFormat2
LASPointFormat2 convert_laszip_to_format2(const laszip_point& laszip_pt) {
  LASPointFormat2 pt;
  static_cast<LASPointFormat0&>(pt) = convert_laszip_to_format0(laszip_pt);
  pt.red = laszip_pt.rgb[0];
  pt.green = laszip_pt.rgb[1];
  pt.blue = laszip_pt.rgb[2];
  return pt;
}

// Helper function to convert laszip_point to LASPointFormat3
LASPointFormat3 convert_laszip_to_format3(const laszip_point& laszip_pt) {
  LASPointFormat3 pt;
  static_cast<LASPointFormat1&>(pt) = convert_laszip_to_format1(laszip_pt);
  pt.red = laszip_pt.rgb[0];
  pt.green = laszip_pt.rgb[1];
  pt.blue = laszip_pt.rgb[2];
  return pt;
}

// Helper function to convert laszip_point to LASPointFormat6
LASPointFormat6 convert_laszip_to_format6(const laszip_point& laszip_pt) {
  LASPointFormat6 pt;
  pt.x = laszip_pt.X;
  pt.y = laszip_pt.Y;
  pt.z = laszip_pt.Z;
  pt.intensity = laszip_pt.intensity;
  pt.return_number = static_cast<uint8_t>(laszip_pt.extended_return_number & 0x0F);
  pt.number_of_returns = static_cast<uint8_t>(laszip_pt.extended_number_of_returns & 0x0F);
  pt.classification_flags = static_cast<uint8_t>(laszip_pt.extended_classification_flags & 0x0F);
  pt.scanner_channel = static_cast<uint8_t>(laszip_pt.extended_scanner_channel & 0x03);
  pt.scan_direction_flag = static_cast<uint8_t>(laszip_pt.scan_direction_flag);
  pt.edge_of_flight_line = static_cast<uint8_t>(laszip_pt.edge_of_flight_line);
  pt.classification = static_cast<LASClassification>(laszip_pt.extended_classification);
  pt.user_data = laszip_pt.user_data;
  pt.scan_angle = laszip_pt.extended_scan_angle;
  pt.point_source_id = laszip_pt.point_source_ID;
  pt.gps_time = laszip_pt.gps_time;
  return pt;
}

// Helper function to convert laszip_point to LASPointFormat7
LASPointFormat7 convert_laszip_to_format7(const laszip_point& laszip_pt) {
  LASPointFormat7 pt;
  static_cast<LASPointFormat6&>(pt) = convert_laszip_to_format6(laszip_pt);
  pt.red = laszip_pt.rgb[0];
  pt.green = laszip_pt.rgb[1];
  pt.blue = laszip_pt.rgb[2];
  return pt;
}

// Helper function to convert laszip point to PointType
template <typename PointType>
PointType convert_laszip_point(const laszip_point& laszip_pt) {
  if constexpr (std::is_same_v<PointType, LASPointFormat0>) {
    return convert_laszip_to_format0(laszip_pt);
  } else if constexpr (std::is_same_v<PointType, LASPointFormat1>) {
    return convert_laszip_to_format1(laszip_pt);
  } else if constexpr (std::is_same_v<PointType, LASPointFormat2>) {
    return convert_laszip_to_format2(laszip_pt);
  } else if constexpr (std::is_same_v<PointType, LASPointFormat3>) {
    return convert_laszip_to_format3(laszip_pt);
  } else if constexpr (std::is_same_v<PointType, LASPointFormat6>) {
    return convert_laszip_to_format6(laszip_pt);
  } else if constexpr (std::is_same_v<PointType, LASPointFormat7>) {
    return convert_laszip_to_format7(laszip_pt);
  } else {
    LASPP_FAIL("Unsupported point format");
  }
}

// Read points with laspp
template <typename PointType>
std::vector<PointType> read_laspp_points(const std::filesystem::path& file_path,
                                         laszip_U64& num_points) {
  std::ifstream ifs(file_path, std::ios::binary);
  LASPP_ASSERT(ifs.is_open(), "Failed to open file: ", file_path);
  LASReader reader(ifs);

  num_points = reader.num_points();
  std::cout << "Reading " << num_points << " points from file: " << file_path << std::endl;

  std::vector<PointType> laspp_points(num_points);
  auto laspp_span =
      reader.read_chunks(std::span<PointType>(laspp_points), {0, reader.num_chunks()});
  LASPP_ASSERT_EQ(laspp_span.size(), num_points, "LAS++ read incorrect number of points");
  return laspp_points;
}

// Open laszip reader and validate point count
template <typename PointType>
laszip_POINTER open_laszip_reader(const std::filesystem::path& file_path, laszip_U64 num_points) {
  laszip_POINTER reader_laszip;
  LASPP_ASSERT_EQ(laszip_create(&reader_laszip), 0, "Failed to create laszip reader");

  laszip_BOOL is_compressed = 0;
  LASPP_ASSERT_EQ(laszip_open_reader(reader_laszip, file_path.string().c_str(), &is_compressed), 0,
                  "Failed to open file with laszip");

  laszip_header* header_laszip;
  LASPP_ASSERT_EQ(laszip_get_header_pointer(reader_laszip, &header_laszip), 0,
                  "Failed to get laszip header");

  laszip_U64 laszip_num_points = header_laszip->extended_number_of_point_records;
  if (laszip_num_points == 0) {
    laszip_num_points = header_laszip->number_of_point_records;
  }
  LASPP_ASSERT_EQ(laszip_num_points, num_points, "Point count mismatch in headers");

  return reader_laszip;
}

// Read all points from laszip reader
template <typename PointType>
std::vector<PointType> read_all_laszip_points(laszip_POINTER reader_laszip, laszip_U64 num_points) {
  laszip_point* laszip_pt;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(reader_laszip, &laszip_pt), 0,
                  "Failed to get laszip point pointer");

  std::vector<PointType> laszip_points;
  laszip_points.reserve(num_points);

  for (laszip_U64 i = 0; i < num_points; ++i) {
    LASPP_ASSERT_EQ(laszip_read_point(reader_laszip), 0, "Failed to read point ", i);
    PointType converted_pt = convert_laszip_point<PointType>(*laszip_pt);
    laszip_points.push_back(converted_pt);
  }

  return laszip_points;
}

template <typename PointType>
std::vector<PointType> read_points_with_laszip(const std::filesystem::path& file_path,
                                               laszip_U64 num_points) {
  laszip_POINTER reader_laszip = open_laszip_reader<PointType>(file_path, num_points);

  std::vector<PointType> laszip_points =
      read_all_laszip_points<PointType>(reader_laszip, num_points);

  LASPP_ASSERT_EQ(laszip_close_reader(reader_laszip), 0, "Failed to close laszip reader");
  LASPP_ASSERT_EQ(laszip_destroy(reader_laszip), 0, "Failed to destroy laszip reader");

  return laszip_points;
}

template <typename PointType>
void compare_and_report_mismatches(const std::vector<PointType>& laspp_points,
                                   const std::vector<PointType>& laszip_points,
                                   laszip_U64 num_points) {
  size_t mismatch_count = 0;
  for (size_t i = 0; i < num_points; ++i) {
    if (laspp_points[i] != laszip_points[i]) {
      if (mismatch_count == 0) {
        std::cerr << "\nMismatch found at point " << i << ":" << std::endl;
        std::cerr << "LAS++ point:" << std::endl;
        std::cerr << laspp_points[i] << std::endl;
        std::cerr << "LASzip point:" << std::endl;
        std::cerr << laszip_points[i] << std::endl;
      }
      ++mismatch_count;
      if (mismatch_count >= 10) {
        std::cerr << "... (showing first 10 mismatches)" << std::endl;
        break;
      }
    }
  }

  if (mismatch_count == 0) {
    std::cout << "✓ All " << num_points << " points match between LAS++ and LASzip!" << std::endl;
  } else {
    std::cerr << "✗ Found " << mismatch_count << " mismatched points out of " << num_points
              << std::endl;
    std::exit(1);
  }
}

template <typename PointType>
void read_and_compare_points(const std::filesystem::path& file_path) {
  laszip_U64 num_points = 0;
  std::vector<PointType> laspp_points = read_laspp_points<PointType>(file_path, num_points);
  std::vector<PointType> laszip_points = read_points_with_laszip<PointType>(file_path, num_points);
  compare_and_report_mismatches(laspp_points, laszip_points, num_points);
}

// Wrapper function to handle different point formats
void compare_file(const std::filesystem::path& file_path) {
  std::ifstream ifs(file_path, std::ios::binary);
  LASPP_ASSERT(ifs.is_open(), "Failed to open file: ", file_path);
  LASReader reader(ifs);

  uint8_t point_format = reader.header().point_format();
  // Remove compression bit
  uint8_t base_format = point_format & uint8_t(~(uint8_t(1u) << 7));

  std::cout << "File: " << file_path << std::endl;
  std::cout << "Point format: " << static_cast<int>(base_format) << std::endl;
  bool is_compressed = (point_format & (uint8_t(1u) << 7)) != 0;
  std::cout << "Compressed: " << (is_compressed ? "yes" : "no") << std::endl;
  std::cout << "Number of points: " << reader.num_points() << std::endl;
  std::cout << std::endl;

  switch (base_format) {
    case 0:
      read_and_compare_points<LASPointFormat0>(file_path);
      break;
    case 1:
      read_and_compare_points<LASPointFormat1>(file_path);
      break;
    case 2:
      read_and_compare_points<LASPointFormat2>(file_path);
      break;
    case 3:
      read_and_compare_points<LASPointFormat3>(file_path);
      break;
    case 4:
    case 5:
      std::cerr << "Error: Point formats 4 and 5 "
                   "(with wave packet data) are not yet supported"
                << std::endl;
      std::exit(1);
      break;
    case 6:
      read_and_compare_points<LASPointFormat6>(file_path);
      break;
    case 7:
      read_and_compare_points<LASPointFormat7>(file_path);
      break;
    case 8:
    case 9:
    case 10:
      std::cerr << "Error: Point formats 8, 9, and 10 "
                   "are not yet supported"
                << std::endl;
      std::exit(1);
      break;
    default:
      std::cerr << "Error: Unsupported point format: " << static_cast<int>(base_format)
                << std::endl;
      std::exit(1);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <las_file>" << std::endl;
    std::cerr << "  Compares data read by LAS++ and LASzip libraries" << std::endl;
    return 1;
  }

  std::filesystem::path file_path(argv[1]);
  if (!std::filesystem::exists(file_path)) {
    std::cerr << "Error: File does not exist: " << file_path << std::endl;
    return 1;
  }

  try {
    compare_file(file_path);
    std::cout << "\n✓ Test passed: All data matches!" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
}
