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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "laszipper.hpp"
#include "laz/laz_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

namespace {

constexpr laszip_U32 ChunkSize = 204;

template <typename PointT>
std::vector<PointT> generate_random_points(size_t n_points, uint32_t seed = 42) {
  std::vector<PointT> points;
  points.reserve(n_points);
  std::mt19937_64 gen(seed);
  for (size_t i = 0; i < n_points; i++) {
    points.emplace_back(PointT::RandomData(gen));
  }
  return points;
}

template <typename PointT>
void populate_laszip_format0_fields(const PointT& point, laszip_point& las_point) {
  unsigned char return_num = static_cast<unsigned char>(point.bit_byte.return_number);
  unsigned char num_returns = static_cast<unsigned char>(point.bit_byte.number_of_returns);
  las_point.return_number = return_num & 0x7;
  las_point.number_of_returns = num_returns & 0x7;
  las_point.scan_direction_flag = point.bit_byte.scan_direction_flag;
  las_point.edge_of_flight_line = point.bit_byte.edge_of_flight_line;
  las_point.classification = static_cast<laszip_U8>(point.classification_byte.classification);
  las_point.synthetic_flag = point.classification_byte.synthetic;
  las_point.keypoint_flag = point.classification_byte.key_point;
  las_point.withheld_flag = point.classification_byte.withheld;
  las_point.scan_angle_rank = static_cast<laszip_I8>(point.scan_angle_rank);
  las_point.user_data = point.user_data;
  las_point.point_source_ID = point.point_source_id;
  las_point.extended_point_type = 0;
}

template <typename PointT>
void populate_laszip_format6_fields(const PointT& point, laszip_point& las_point) {
  las_point.return_number = static_cast<laszip_U8>(std::min<int>(point.return_number, 7)) & 0x7;
  las_point.number_of_returns =
      static_cast<laszip_U8>(std::min<int>(point.number_of_returns, 7)) & 0x7;
  las_point.scan_direction_flag = point.scan_direction_flag;
  las_point.edge_of_flight_line = point.edge_of_flight_line;
  las_point.synthetic_flag = point.classification_flags & 0x1;
  las_point.keypoint_flag = (point.classification_flags >> 1) & 0x1;
  las_point.withheld_flag = (point.classification_flags >> 2) & 0x1;
  las_point.extended_point_type = 1;
  las_point.extended_classification_flags = point.classification_flags;
  las_point.extended_classification = static_cast<laszip_U8>(point.classification);
  las_point.extended_return_number = point.return_number;
  las_point.extended_number_of_returns = point.number_of_returns;
  las_point.extended_scanner_channel = point.scanner_channel;
  las_point.extended_scan_angle = static_cast<laszip_I16>(point.scan_angle);
  las_point.scan_angle_rank =
      static_cast<laszip_I8>(std::clamp<int>(static_cast<int>(point.scan_angle) / 512, -128, 127));
  las_point.classification = static_cast<laszip_U8>(point.classification) & 0x1F;
  las_point.user_data = point.user_data;
  las_point.point_source_ID = point.point_source_id;
  las_point.gps_time = point.gps_time;
}

template <typename PointT>
void populate_laszip_point(const PointT& point, laszip_point& las_point) {
  las_point.X = point.x;
  las_point.Y = point.y;
  las_point.Z = point.z;
  las_point.intensity = point.intensity;
  las_point.extra_bytes = nullptr;

  if constexpr (std::is_base_of_v<LASPointFormat0, PointT>) {
    populate_laszip_format0_fields(point, las_point);
  }

  if constexpr (std::is_base_of_v<GPSTime, PointT>) {
    las_point.gps_time = static_cast<double>(static_cast<const GPSTime&>(point));
  }

  if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
    populate_laszip_format6_fields(point, las_point);
  }

  if constexpr (std::is_base_of_v<ColorData, PointT>) {
    las_point.rgb[0] = point.red;
    las_point.rgb[1] = point.green;
    las_point.rgb[2] = point.blue;
  }
}

// Conversion functions from laszip_point to PointT
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

LASPointFormat1 convert_laszip_to_format1(const laszip_point& laszip_pt) {
  LASPointFormat1 pt;
  static_cast<LASPointFormat0&>(pt) = convert_laszip_to_format0(laszip_pt);
  pt.gps_time.f64 = laszip_pt.gps_time;
  return pt;
}

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

LASPointFormat7 convert_laszip_to_format7(const laszip_point& laszip_pt) {
  LASPointFormat7 pt;
  static_cast<LASPointFormat6&>(pt) = convert_laszip_to_format6(laszip_pt);
  pt.red = laszip_pt.rgb[0];
  pt.green = laszip_pt.rgb[1];
  pt.blue = laszip_pt.rgb[2];
  return pt;
}

template <typename PointT>
PointT convert_laszip_point(const laszip_point& laszip_pt) {
  if constexpr (std::is_same_v<PointT, LASPointFormat0>) {
    return convert_laszip_to_format0(laszip_pt);
  } else if constexpr (std::is_same_v<PointT, LASPointFormat1>) {
    return convert_laszip_to_format1(laszip_pt);
  } else if constexpr (std::is_same_v<PointT, LASPointFormat6>) {
    return convert_laszip_to_format6(laszip_pt);
  } else if constexpr (std::is_same_v<PointT, LASPointFormat7>) {
    return convert_laszip_to_format7(laszip_pt);
  } else {
    LASPP_FAIL("Unsupported point format for conversion");
  }
}

template <typename PointT>
void populate_header_basic_fields(laszip_header& header, laszip_U64 total_points) {
  header.version_major = 1;
  header.version_minor = PointT::MinVersion;
  header.header_size = (header.version_minor >= 4) ? 375 : 227;
  header.offset_to_point_data = header.header_size;
  header.point_data_format = PointT::PointFormat;
  header.point_data_record_length = static_cast<laszip_U16>(sizeof(PointT));

  header.number_of_point_records = static_cast<laszip_U32>(
      std::min<laszip_U64>(total_points, std::numeric_limits<laszip_U32>::max()));
  header.extended_number_of_point_records = total_points;

  std::fill(std::begin(header.number_of_points_by_return),
            std::end(header.number_of_points_by_return), 0);
  std::fill(std::begin(header.extended_number_of_points_by_return),
            std::end(header.extended_number_of_points_by_return), 0);

  const double scale = 0.27;
  header.x_scale_factor = scale;
  header.y_scale_factor = scale;
  header.z_scale_factor = scale;
  header.x_offset = 146;
  header.y_offset = -25.4;
  header.z_offset = 512;
}

template <typename PointT>
void populate_header_bounds(const std::vector<PointT>& points, laszip_header& header) {
  int32_t min_x = std::numeric_limits<int32_t>::max();
  int32_t max_x = std::numeric_limits<int32_t>::lowest();
  int32_t min_y = min_x;
  int32_t max_y = max_x;
  int32_t min_z = min_x;
  int32_t max_z = max_x;
  for (const PointT& point : points) {
    const int32_t x = point.x;
    const int32_t y = point.y;
    const int32_t z = point.z;
    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
    min_z = std::min(min_z, z);
    max_x = std::max(max_x, x);
    max_y = std::max(max_y, y);
    max_z = std::max(max_z, z);

    auto increment_return_counters = [&](uint8_t return_number, uint8_t number_of_returns) {
      if (return_number == 0 || number_of_returns == 0) {
        return;
      }
      uint8_t basic_index = std::min<uint8_t>(return_number, 5);
      header.number_of_points_by_return[basic_index - 1]++;
      uint8_t extended_index = std::min<uint8_t>(return_number, 15);
      header.extended_number_of_points_by_return[extended_index - 1]++;
    };

    if constexpr (std::is_base_of_v<LASPointFormat0, PointT>) {
      increment_return_counters(point.bit_byte.return_number, point.bit_byte.number_of_returns);
    }
    if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
      increment_return_counters(point.return_number, point.number_of_returns);
    }
  }
  header.min_x = header.x_offset + min_x * header.x_scale_factor;
  header.max_x = header.x_offset + max_x * header.x_scale_factor;
  header.min_y = header.y_offset + min_y * header.y_scale_factor;
  header.max_y = header.y_offset + max_y * header.y_scale_factor;
  header.min_z = header.z_offset + min_z * header.z_scale_factor;
  header.max_z = header.z_offset + max_z * header.z_scale_factor;
}

template <typename PointT>
void populate_header(const std::vector<PointT>& points, laszip_header& header) {
  std::memset(&header, 0, sizeof(laszip_header));
  laszip_U64 total_points = static_cast<laszip_U64>(points.size());
  populate_header_basic_fields<PointT>(header, total_points);
  populate_header_bounds(points, header);

  std::string system_identifier = "laspp-tests";
  std::string generating_software = "laspp-tests";
  std::snprintf(header.system_identifier, sizeof(header.system_identifier), "%s",
                system_identifier.c_str());
  std::snprintf(header.generating_software, sizeof(header.generating_software), "%s",
                generating_software.c_str());
}

class TempFile {
 public:
  explicit TempFile(const std::string& prefix) {
    auto base_dir = std::filesystem::temp_directory_path() / "laspp_laszip_tests";
    std::filesystem::create_directories(base_dir);
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = base_dir / (prefix + "_" + std::to_string(timestamp) + ".laz");
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

template <typename PointT>
void write_points_with_laszip(const std::vector<PointT>& points, const std::filesystem::path& path,
                              bool request_native_extension) {
  laszip_POINTER writer;
  LASPP_ASSERT_EQ(laszip_create(&writer), 0);

  // Configure native extension or compatibility mode
  // Note: compatibility mode (format 6+ stored as format 0-5 +
  // extra bytes) is not currently supported by the LAS++ reader,
  // which only supports native extension mode
  if (request_native_extension) {
    LASPP_ASSERT_EQ(laszip_request_native_extension(writer, 1), 0);
  } else if constexpr (PointT::PointFormat > 5) {
    LASPP_ASSERT_EQ(laszip_request_compatibility_mode(writer, 1), 0);
  }

  laszip_header* header;
  LASPP_ASSERT_EQ(laszip_get_header_pointer(writer, &header), 0);
  populate_header(points, *header);
  LASPP_ASSERT_EQ(laszip_set_header(writer, header), 0);

  LASPP_ASSERT_EQ(laszip_set_chunk_size(writer, ChunkSize), 0);

  LASPP_ASSERT_EQ(laszip_open_writer(writer, path.string().c_str(), 1), 0);

  laszip_point* las_point;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(writer, &las_point), 0);

  for (size_t idx = 0; idx < points.size(); idx++) {
    const PointT& point = points[idx];
    populate_laszip_point(point, *las_point);
    LASPP_ASSERT_EQ(laszip_set_point(writer, las_point), 0);
    LASPP_ASSERT_EQ(laszip_write_point(writer), 0, "Failed to write point ", idx);
  }

  LASPP_ASSERT_EQ(laszip_close_writer(writer), 0);
  LASPP_ASSERT_EQ(laszip_destroy(writer), 0);
}

// Write points with LAS++ writer
template <typename PointT>
void write_points_with_laspp(const std::vector<PointT>& points, const std::filesystem::path& path) {
  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  LASPP_ASSERT(file.is_open(), "Failed to open file for writing");

  // Enable LAZ compression by setting bit 128 in the point format
  uint8_t compressed_point_format = PointT::PointFormat | 128;
  LASWriter writer(file, compressed_point_format);

  // Verify compression is enabled
  LASPP_ASSERT(writer.header().is_laz_compressed(), "File should be LAZ compressed");

  // Set scale factors and offsets (LASWriter handles point counts and bounds automatically)
  const double scale = 0.27;
  writer.header().transform().m_scale_factors.x() = scale;
  writer.header().transform().m_scale_factors.y() = scale;
  writer.header().transform().m_scale_factors.z() = scale;
  writer.header().transform().m_offsets.x() = 146;
  writer.header().transform().m_offsets.y() = -25.4;
  writer.header().transform().m_offsets.z() = 512;

  writer.write_points(std::span<const PointT>(points.data(), points.size()), ChunkSize);
}

// Read points with laszip API
template <typename PointT>
std::vector<PointT> read_points_with_laszip(const std::filesystem::path& path) {
  laszip_POINTER reader;
  LASPP_ASSERT_EQ(laszip_create(&reader), 0, "Failed to create laszip reader");

  laszip_BOOL is_compressed = 0;
  LASPP_ASSERT_EQ(laszip_open_reader(reader, path.string().c_str(), &is_compressed), 0,
                  "Failed to open file with laszip");

  laszip_header* header;
  LASPP_ASSERT_EQ(laszip_get_header_pointer(reader, &header), 0, "Failed to get laszip header");

  laszip_U64 num_points = header->extended_number_of_point_records;
  if (num_points == 0) {
    num_points = header->number_of_point_records;
  }

  laszip_point* las_point;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(reader, &las_point), 0,
                  "Failed to get laszip point pointer");

  // Verify point format matches what we expect
  LASPP_ASSERT_EQ(header->point_data_format, PointT::PointFormat,
                  "Point format mismatch: header says ", header->point_data_format,
                  " but expected ", PointT::PointFormat);

  std::vector<PointT> decoded_points;
  decoded_points.reserve(num_points);

  for (laszip_U64 i = 0; i < num_points; ++i) {
    laszip_I32 error = laszip_read_point(reader);
    LASPP_ASSERT_EQ(error, 0, "Failed to read point ", i, " laszip error: ", error);

    decoded_points.push_back(convert_laszip_point<PointT>(*las_point));
  }

  LASPP_ASSERT_EQ(laszip_close_reader(reader), 0, "Failed to close laszip reader");
  LASPP_ASSERT_EQ(laszip_destroy(reader), 0, "Failed to destroy laszip reader");

  return decoded_points;
}

// Test roundtrip: write with LASzip, read with LAS++ reader
template <typename PointT>
void run_laszip_file_roundtrip(size_t n_points, bool request_native_extension) {
  auto points = generate_random_points<PointT>(n_points);
  TempFile temp_file("laszip_roundtrip");

  write_points_with_laszip(points, temp_file.path(), request_native_extension);

  // Read with LAS++ reader
  std::ifstream laz_file(temp_file.path(), std::ios::binary);
  LASPP_ASSERT(laz_file.is_open(), "Failed to open file for reading");
  LASReader reader(laz_file);

  LASPP_ASSERT_EQ(reader.header().num_points(), points.size(),
                  "Point count mismatch in LAS++ reader header");

  std::vector<PointT> decoded(points.size());
  auto decoded_span = reader.read_chunks(std::span<PointT>(decoded), {0, reader.num_chunks()});
  LASPP_ASSERT_EQ(decoded_span.size(), points.size(), "Decoded point count mismatch");

  // Verify all points match
  for (size_t i = 0; i < points.size(); ++i) {
    LASPP_ASSERT_EQ(decoded_span[i], points[i], "Point ", i, " mismatch in roundtrip test");
  }
}

// Test roundtrip: write with LAS++, read with laszip
template <typename PointT>
void run_laspp_file_roundtrip(size_t n_points) {
  auto points = generate_random_points<PointT>(n_points);
  TempFile temp_file("laspp_roundtrip");

  write_points_with_laspp(points, temp_file.path());

  // First verify the file was written correctly by reading with LAS++
  {
    std::ifstream laz_file(temp_file.path(), std::ios::binary);
    LASPP_ASSERT(laz_file.is_open(), "Failed to open file for reading with LAS++");
    LASReader reader(laz_file);
    LASPP_ASSERT_EQ(reader.header().num_points(), points.size(),
                    "Point count mismatch in LAS++ reader header");
    std::vector<PointT> laspp_decoded(points.size());
    auto laspp_decoded_span =
        reader.read_chunks(std::span<PointT>(laspp_decoded), {0, reader.num_chunks()});
    LASPP_ASSERT_EQ(laspp_decoded_span.size(), points.size(), "LAS++ decoded point count mismatch");

    // Verify LAS++ can read its own file
    for (size_t i = 0; i < points.size(); ++i) {
      LASPP_ASSERT_EQ(laspp_decoded_span[i], points[i], "Point ", i,
                      " mismatch when LAS++ reads its own file");
    }
  }

  // Now read with laszip
  std::vector<PointT> decoded_points = read_points_with_laszip<PointT>(temp_file.path());

  LASPP_ASSERT_EQ(decoded_points.size(), points.size(), "Decoded point count mismatch");

  // Verify all points match
  for (size_t i = 0; i < points.size(); ++i) {
    LASPP_ASSERT_EQ(decoded_points[i], points[i], "Point ", i, " mismatch in LAS++ roundtrip test");
  }
}

// Test roundtrip using LAS++ internal compression (no file I/O)
template <typename PointT>
void run_laszip_internal_roundtrip(size_t n_points) {
  auto points = generate_random_points<PointT>(n_points);

  LASzip laszip;
  LASPP_ASSERT(laszip.setup(
      PointT::PointFormat, sizeof(PointT),
      PointT::PointFormat >= 6 ? LASZIP_COMPRESSOR_LAYERED_CHUNKED : LASZIP_COMPRESSOR_CHUNKED));

  unsigned char* vlr_bytes = nullptr;
  int vlr_size = 0;
  LASPP_ASSERT(laszip.pack(vlr_bytes, vlr_size));

  std::stringstream compressed_stream(std::ios::in | std::ios::out | std::ios::binary);
  LASzipper zipper;
  LASPP_ASSERT(zipper.open(compressed_stream, &laszip));

  if (PointT::PointFormat < 6) {
    PointT point_buffer{};
    std::vector<unsigned char*> point_items(laszip.num_items);
    size_t offset = 0;
    for (unsigned int i = 0; i < laszip.num_items; i++) {
      point_items[i] = reinterpret_cast<unsigned char*>(&point_buffer) + offset;
      offset += laszip.items[i].size;
    }
    LASPP_ASSERT_EQ(offset, sizeof(PointT));
    for (const PointT& point : points) {
      std::memcpy(&point_buffer, &point, sizeof(PointT));
      LASPP_ASSERT(zipper.write(point_items.data()));
    }
  } else {
    laszip_point laszip_point_instance;
    std::vector<unsigned char*> point_items(laszip.num_items);
    for (unsigned int i = 0; i < laszip.num_items; i++) {
      if (laszip.items[i].type == LASitem::POINT14) {
        point_items[i] = reinterpret_cast<unsigned char*>(&laszip_point_instance);
        continue;
      }
      if constexpr (std::is_base_of_v<ColorData, PointT>) {
        if (laszip.items[i].type == LASitem::RGB14) {
          point_items[i] = reinterpret_cast<unsigned char*>(&laszip_point_instance.rgb);
          continue;
        }
      }
      LASPP_FAIL("Unexpected item type in LASzip items for point format >= 6");
    }
    for (const PointT& point : points) {
      populate_laszip_point(point, laszip_point_instance);
      LASPP_ASSERT(zipper.write(point_items.data()));
    }
  }
  LASPP_ASSERT(zipper.close());

  std::string compressed = compressed_stream.str();
  LASPP_ASSERT_GE(compressed.size(), sizeof(uint64_t));

  uint64_t chunk_table_offset = 0;
  std::memcpy(&chunk_table_offset, compressed.data(), sizeof(chunk_table_offset));
  LASPP_ASSERT(chunk_table_offset >= sizeof(uint64_t));
  LASPP_ASSERT_LE(chunk_table_offset, compressed.size());

  std::vector<std::byte> chunk_data(chunk_table_offset - sizeof(uint64_t));
  std::copy(compressed.begin() + sizeof(uint64_t),
            compressed.begin() + static_cast<int64_t>(chunk_table_offset),
            reinterpret_cast<char*>(chunk_data.data()));

  std::string vlr_string(reinterpret_cast<char*>(vlr_bytes), static_cast<size_t>(vlr_size));
  std::stringstream vlr_stream(vlr_string);
  LAZSpecialVLRContent laz_vlr(vlr_stream);

  std::vector<PointT> decoded(points.size());
  LAZReader laz_reader(laz_vlr);
  laz_reader.decompress_chunk(std::span<std::byte>(chunk_data), std::span<PointT>(decoded));

  for (size_t i = 0; i < points.size(); i++) {
    LASPP_ASSERT_EQ(decoded[i], points[i], "Point ", i, " mismatch in internal roundtrip");
  }
}

}  // namespace

int main() {
  constexpr size_t SmallPointCount = 128;
  constexpr size_t MediumPointCount = 500;
  constexpr size_t LargePointCount = 5000;

  // Format 0 tests - basic point format
  run_laszip_internal_roundtrip<LASPointFormat0>(256);
  run_laszip_file_roundtrip<LASPointFormat0>(1, false);
  run_laszip_file_roundtrip<LASPointFormat0>(SmallPointCount, false);
  run_laszip_file_roundtrip<LASPointFormat0>(MediumPointCount, false);
  run_laspp_file_roundtrip<LASPointFormat0>(1);
  run_laspp_file_roundtrip<LASPointFormat0>(SmallPointCount);
  run_laspp_file_roundtrip<LASPointFormat0>(MediumPointCount);

  // Format 1 tests - with GPS time
  run_laszip_internal_roundtrip<LASPointFormat1>(1000);
  run_laszip_file_roundtrip<LASPointFormat1>(1, false);
  run_laszip_file_roundtrip<LASPointFormat1>(SmallPointCount, false);
  run_laszip_file_roundtrip<LASPointFormat1>(MediumPointCount, false);
  run_laszip_file_roundtrip<LASPointFormat1>(LargePointCount, false);
  run_laspp_file_roundtrip<LASPointFormat1>(1);
  run_laspp_file_roundtrip<LASPointFormat1>(SmallPointCount);
  run_laspp_file_roundtrip<LASPointFormat1>(MediumPointCount);
  run_laspp_file_roundtrip<LASPointFormat1>(LargePointCount);

  // Format 6 tests - extended point format with native extension
  // Note: compatibility mode (request_native_extension=false)
  // is not supported by LAS++ reader - it only supports native
  // extension mode (Point14 items)
  run_laszip_internal_roundtrip<LASPointFormat6>(1000);
  run_laszip_file_roundtrip<LASPointFormat6>(1, true);
  run_laszip_file_roundtrip<LASPointFormat6>(SmallPointCount, true);
  run_laszip_file_roundtrip<LASPointFormat6>(MediumPointCount, true);
  run_laszip_file_roundtrip<LASPointFormat6>(LargePointCount, true);
  run_laspp_file_roundtrip<LASPointFormat6>(1);
  run_laspp_file_roundtrip<LASPointFormat6>(SmallPointCount);
  run_laspp_file_roundtrip<LASPointFormat6>(MediumPointCount);
  run_laspp_file_roundtrip<LASPointFormat6>(LargePointCount);

  // Format 7 tests - extended point format with RGB colors
  run_laszip_internal_roundtrip<LASPointFormat7>(1000);
  run_laszip_file_roundtrip<LASPointFormat7>(1, true);
  run_laszip_file_roundtrip<LASPointFormat7>(SmallPointCount, true);
  run_laszip_file_roundtrip<LASPointFormat7>(MediumPointCount, true);
  run_laszip_file_roundtrip<LASPointFormat7>(LargePointCount, true);
  run_laspp_file_roundtrip<LASPointFormat7>(1);
  run_laspp_file_roundtrip<LASPointFormat7>(SmallPointCount);
  run_laspp_file_roundtrip<LASPointFormat7>(MediumPointCount);
  run_laspp_file_roundtrip<LASPointFormat7>(LargePointCount);

  return 0;
}
