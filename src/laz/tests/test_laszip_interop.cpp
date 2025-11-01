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

#include <laszip/laszip_api.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "laszip.hpp"
#include "laszipper.hpp"
#include "laz/laz_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

namespace {

template <typename PointT>
struct PointFormatTraits;

template <>
struct PointFormatTraits<LASPointFormat0> {
  static constexpr unsigned char kPointFormat = 0;
  static constexpr unsigned short kCompressor = LASZIP_COMPRESSOR_CHUNKED;
  static constexpr unsigned char kVersionMinor = 2;
};

template <>
struct PointFormatTraits<LASPointFormat1> {
  static constexpr unsigned char kPointFormat = 1;
  static constexpr unsigned short kCompressor = LASZIP_COMPRESSOR_CHUNKED;
  static constexpr unsigned char kVersionMinor = 2;
};

template <>
struct PointFormatTraits<LASPointFormat6> {
  static constexpr unsigned char kPointFormat = 6;
  static constexpr unsigned short kCompressor = LASZIP_COMPRESSOR_LAYERED_CHUNKED;
  static constexpr unsigned char kVersionMinor = 4;
};

template <>
struct PointFormatTraits<LASPointFormat7> {
  static constexpr unsigned char kPointFormat = 7;
  static constexpr unsigned short kCompressor = LASZIP_COMPRESSOR_LAYERED_CHUNKED;
  static constexpr unsigned char kVersionMinor = 4;
};

template <typename PointT>
std::vector<PointT> generate_points(size_t n_points) {
  std::vector<PointT> points;
  points.reserve(n_points);

  std::mt19937 gen(0);
  std::uniform_int_distribution<int32_t> coord_dist(-500000, 500000);
  std::uniform_int_distribution<uint16_t> intensity_dist(0, std::numeric_limits<uint16_t>::max());
  std::uniform_int_distribution<uint16_t> point_source_dist(0,
                                                            std::numeric_limits<uint16_t>::max());
  std::uniform_int_distribution<uint8_t> returns_dist(1, 15);
  std::uniform_int_distribution<uint8_t> bool_dist(0, 1);
  std::uniform_int_distribution<uint8_t> classification_dist(0, 22);
  std::uniform_int_distribution<uint8_t> flags_dist(0, 15);
  std::uniform_int_distribution<uint8_t> user_dist(0, std::numeric_limits<uint8_t>::max());
  std::uniform_int_distribution<uint16_t> angle_dist(0, 65535);
  std::uniform_int_distribution<uint16_t> color_dist(0, std::numeric_limits<uint16_t>::max());
  std::uniform_real_distribution<double> gps_dist(-1e6, 1e6);

  for (size_t i = 0; i < n_points; i++) {
    PointT point{};

    point.x = coord_dist(gen);
    point.y = coord_dist(gen);
    point.z = coord_dist(gen);
    point.intensity = intensity_dist(gen);

    if constexpr (std::is_base_of_v<LASPointFormat0, PointT>) {
      uint8_t number_of_returns = static_cast<uint8_t>(returns_dist(gen) & 0x0F);
      number_of_returns = std::max<uint8_t>(number_of_returns, 1);
      uint8_t return_number = static_cast<uint8_t>(returns_dist(gen) & 0x0F);
      return_number = std::max<uint8_t>(return_number, 1);
      return_number = std::min(return_number, number_of_returns);
      point.bit_byte.number_of_returns = static_cast<uint8_t>(number_of_returns & 0x07);
      point.bit_byte.return_number = static_cast<uint8_t>(return_number & 0x07);
      point.bit_byte.scan_direction_flag = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.bit_byte.edge_of_flight_line = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.classification_byte.classification =
          static_cast<LASClassification>(classification_dist(gen));
      point.classification_byte.synthetic = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.classification_byte.key_point = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.classification_byte.withheld = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.scan_angle_rank = static_cast<uint8_t>(angle_dist(gen) & 0xFF);
      point.user_data = user_dist(gen);
      point.point_source_id = point_source_dist(gen);
    }

    if constexpr (std::is_base_of_v<GPSTime, PointT>) {
      static_cast<GPSTime&>(point).gps_time.f64 = gps_dist(gen);
    }

    if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
      uint8_t return_number = static_cast<uint8_t>((returns_dist(gen) % 4) + 1);
      uint8_t number_of_returns = static_cast<uint8_t>((returns_dist(gen) % 4) + 1);
      if (return_number > number_of_returns) {
        return_number = number_of_returns;
      }
      point.return_number = return_number & 0x0F;
      point.number_of_returns = number_of_returns & 0x0F;
      uint8_t synthetic = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      uint8_t keypoint = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      uint8_t withheld = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      uint8_t overlap = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.classification_flags =
          static_cast<uint8_t>((overlap << 3) | (withheld << 2) | (keypoint << 1) | synthetic) &
          0x0F;
      point.classification_flags &= 0x07;
      point.scanner_channel = static_cast<uint8_t>(bool_dist(gen) & 0x3);
      point.scan_direction_flag = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.edge_of_flight_line = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      point.classification = static_cast<LASClassification>(classification_dist(gen));
      point.user_data = user_dist(gen);
      point.scan_angle = static_cast<int16_t>(angle_dist(gen));
      point.point_source_id = point_source_dist(gen);
      point.gps_time = gps_dist(gen);
    }

    if constexpr (std::is_base_of_v<ColorData, PointT>) {
      point.red = color_dist(gen);
      point.green = color_dist(gen);
      point.blue = color_dist(gen);
    }

    points.push_back(point);
  }

  return points;
}

template <typename PointT>
void run_laszip_roundtrip(size_t n_points) {
  auto points = generate_points<PointT>(n_points);

  LASzip laszip;
  LASPP_ASSERT(laszip.setup(PointFormatTraits<PointT>::kPointFormat, sizeof(PointT),
                            PointFormatTraits<PointT>::kCompressor));

  unsigned char* vlr_bytes = nullptr;
  int vlr_size = 0;
  LASPP_ASSERT(laszip.pack(vlr_bytes, vlr_size));

  std::stringstream compressed_stream(std::ios::in | std::ios::out | std::ios::binary);
  LASzipper zipper;
  LASPP_ASSERT(zipper.open(compressed_stream, &laszip));

  std::vector<unsigned char> point_buffer(sizeof(PointT));
  std::vector<unsigned char*> point_items(laszip.num_items);
  size_t offset = 0;
  for (unsigned int i = 0; i < laszip.num_items; i++) {
    point_items[i] = point_buffer.data() + offset;
    offset += laszip.items[i].size;
  }
  LASPP_ASSERT_EQ(offset, point_buffer.size());

  for (const PointT& point : points) {
    std::memcpy(point_buffer.data(), &point, sizeof(PointT));
    LASPP_ASSERT(zipper.write(point_items.data()));
  }
  LASPP_ASSERT(zipper.close());

  std::string compressed = compressed_stream.str();
  LASPP_ASSERT_GE(compressed.size(), sizeof(uint64_t));

  uint64_t chunk_table_offset = 0;
  std::memcpy(&chunk_table_offset, compressed.data(), sizeof(uint64_t));
  LASPP_ASSERT(chunk_table_offset >= sizeof(uint64_t));
  LASPP_ASSERT_LE(chunk_table_offset, compressed.size());

  std::vector<std::byte> chunk_data(chunk_table_offset - sizeof(uint64_t));
  std::memcpy(chunk_data.data(), compressed.data() + sizeof(uint64_t), chunk_data.size());

  std::string vlr_string(reinterpret_cast<char*>(vlr_bytes), static_cast<size_t>(vlr_size));

  std::stringstream vlr_stream(vlr_string);
  LAZSpecialVLRContent laz_vlr(vlr_stream);

  std::vector<PointT> decoded(points.size());
  LAZReader reader(laz_vlr);
  reader.decompress_chunk(std::span<std::byte>(chunk_data), std::span<PointT>(decoded));

  for (size_t i = 0; i < points.size(); i++) {
    LASPP_ASSERT_EQ(decoded[i], points[i]);
  }
}

template <typename PointT>
constexpr laszip_U8 extended_point_type() {
  if constexpr (PointFormatTraits<PointT>::kPointFormat >= 6) {
    // IMPORTANT: extended_point_type is a FLAG, not the actual point format!
    // For LAS 1.4 point formats (6+), this should be non-zero to indicate
    // that the point has extended fields (scanner_channel, etc.).
    // LASzip uses this to decide whether to copy extended fields or set them to 0.
    // We use (format - 5) so format 6 -> 1, format 7 -> 2, etc.
    return static_cast<laszip_U8>(PointFormatTraits<PointT>::kPointFormat - 5);
  }
  return 0;
}

template <typename PointT>
void populate_laszip_point(const PointT& point, laszip_point& las_point) {
  las_point.X = point.x;
  las_point.Y = point.y;
  las_point.Z = point.z;
  las_point.intensity = point.intensity;

  las_point.return_number = 0;
  las_point.number_of_returns = 0;
  las_point.scan_direction_flag = 0;
  las_point.edge_of_flight_line = 0;
  las_point.classification = 0;
  las_point.synthetic_flag = 0;
  las_point.keypoint_flag = 0;
  las_point.withheld_flag = 0;
  las_point.scan_angle_rank = 0;
  las_point.user_data = 0;
  las_point.point_source_ID = 0;
  las_point.extended_scan_angle = 0;
  las_point.extended_scanner_channel = 0;
  las_point.extended_classification_flags = 0;
  las_point.extended_classification = 0;
  las_point.extended_return_number = 0;
  las_point.extended_number_of_returns = 0;
  las_point.gps_time = 0.0;
  las_point.rgb[0] = las_point.rgb[1] = las_point.rgb[2] = las_point.rgb[3] = 0;
  las_point.num_extra_bytes = 0;
  las_point.extra_bytes = nullptr;

  if constexpr (std::is_base_of_v<LASPointFormat0, PointT>) {
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
  }

  if constexpr (std::is_base_of_v<GPSTime, PointT>) {
    las_point.gps_time = static_cast<double>(static_cast<const GPSTime&>(point));
  }

  if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
    las_point.return_number = static_cast<laszip_U8>(std::min<int>(point.return_number, 7)) & 0x7;
    las_point.number_of_returns =
        static_cast<laszip_U8>(std::min<int>(point.number_of_returns, 7)) & 0x7;
    las_point.scan_direction_flag = point.scan_direction_flag;
    las_point.edge_of_flight_line = point.edge_of_flight_line;
    las_point.synthetic_flag = point.classification_flags & 0x1;
    las_point.keypoint_flag = (point.classification_flags >> 1) & 0x1;
    las_point.withheld_flag = (point.classification_flags >> 2) & 0x1;
    las_point.extended_classification_flags = point.classification_flags;
    las_point.extended_classification = static_cast<laszip_U8>(point.classification);
    las_point.extended_return_number = point.return_number;
    las_point.extended_number_of_returns = point.number_of_returns;
    las_point.extended_scanner_channel = point.scanner_channel;
    las_point.extended_scan_angle = static_cast<laszip_I16>(point.scan_angle);
    las_point.scan_angle_rank = static_cast<laszip_I8>(
        std::clamp<int>(static_cast<int>(point.scan_angle) / 512, -128, 127));
    las_point.classification = static_cast<laszip_U8>(point.classification) & 0x1F;
    las_point.user_data = point.user_data;
    las_point.point_source_ID = point.point_source_id;
    las_point.gps_time = point.gps_time;
  }

  if constexpr (std::is_base_of_v<ColorData, PointT>) {
    las_point.rgb[0] = point.red;
    las_point.rgb[1] = point.green;
    las_point.rgb[2] = point.blue;
  }
}

template <typename PointT>
void populate_header(const std::vector<PointT>& points, laszip_header& header) {
  std::memset(&header, 0, sizeof(laszip_header));
  header.version_major = 1;
  header.version_minor = PointFormatTraits<PointT>::kVersionMinor;
  header.header_size = (header.version_minor >= 4) ? 375 : 227;
  header.offset_to_point_data = header.header_size;
  header.point_data_format = PointFormatTraits<PointT>::kPointFormat;
  header.point_data_record_length = static_cast<laszip_U16>(sizeof(PointT));

  // Set point counts to the actual values
  laszip_U64 total_points = static_cast<laszip_U64>(points.size());
  header.number_of_point_records = static_cast<laszip_U32>(
      std::min<laszip_U64>(total_points, std::numeric_limits<laszip_U32>::max()));
  header.extended_number_of_point_records = total_points;

  std::fill(std::begin(header.number_of_points_by_return),
            std::end(header.number_of_points_by_return), 0);
  std::fill(std::begin(header.extended_number_of_points_by_return),
            std::end(header.extended_number_of_points_by_return), 0);

  const double scale = 0.01;
  header.x_scale_factor = header.y_scale_factor = header.z_scale_factor = scale;
  header.x_offset = header.y_offset = header.z_offset = 0.0;

  int32_t min_x = std::numeric_limits<int32_t>::max();
  int32_t max_x = std::numeric_limits<int32_t>::lowest();
  int32_t min_y = min_x;
  int32_t max_y = max_x;
  int32_t min_z = min_x;
  int32_t max_z = max_x;

  for (const PointT& point : points) {
    min_x = std::min(min_x, point.x);
    min_y = std::min(min_y, point.y);
    min_z = std::min(min_z, point.z);
    max_x = std::max(max_x, point.x);
    max_y = std::max(max_y, point.y);
    max_z = std::max(max_z, point.z);

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

  header.min_x = header.x_offset + min_x * scale;
  header.max_x = header.x_offset + max_x * scale;
  header.min_y = header.y_offset + min_y * scale;
  header.max_y = header.y_offset + max_y * scale;
  header.min_z = header.z_offset + min_z * scale;
  header.max_z = header.z_offset + max_z * scale;

  std::string system_identifier = "laspp-tests";
  std::string generating_software = "laspp-tests";
  std::strncpy(header.system_identifier, system_identifier.c_str(),
               sizeof(header.system_identifier) - 1);
  std::strncpy(header.generating_software, generating_software.c_str(),
               sizeof(header.generating_software) - 1);
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

  if (request_native_extension) {
    LASPP_ASSERT_EQ(laszip_request_native_extension(writer, 1), 0);
  } else if constexpr (PointFormatTraits<PointT>::kPointFormat > 5) {
    LASPP_ASSERT_EQ(laszip_request_compatibility_mode(writer, 1), 0);
  }

  laszip_header* header;
  LASPP_ASSERT_EQ(laszip_get_header_pointer(writer, &header), 0);
  populate_header(points, *header);
  LASPP_ASSERT_EQ(laszip_set_header(writer, header), 0);

  LASPP_ASSERT_EQ(laszip_set_chunk_size(writer, 5000), 0);

  LASPP_ASSERT_EQ(laszip_open_writer(writer, path.string().c_str(), 1), 0);

  laszip_point* las_point;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(writer, &las_point), 0);

  for (size_t idx = 0; idx < points.size(); idx++) {
    const PointT& point = points[idx];
    populate_laszip_point(point, *las_point);
    LASPP_ASSERT_EQ(laszip_set_point(writer, las_point), 0);
    LASPP_ASSERT_EQ(laszip_write_point(writer), 0);
  }
  LASPP_ASSERT_EQ(laszip_close_writer(writer), 0);
  LASPP_ASSERT_EQ(laszip_destroy(writer), 0);
}

template <typename PointT>
void run_laszip_file_roundtrip(size_t n_points, bool request_native_extension) {
  auto points = generate_points<PointT>(n_points);
  TempFile temp_file("laszip_roundtrip");

  write_points_with_laszip(points, temp_file.path(), request_native_extension);

  // First, verify what LASzip actually wrote by reading back with LASzip
  if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
    laszip_POINTER reader;
    LASPP_ASSERT_EQ(laszip_create(&reader), 0);
    laszip_BOOL is_compressed = 0;
    LASPP_ASSERT_EQ(laszip_open_reader(reader, temp_file.path().string().c_str(), &is_compressed),
                    0);

    laszip_header* header;
    LASPP_ASSERT_EQ(laszip_get_header_pointer(reader, &header), 0);

    laszip_point* las_point;
    LASPP_ASSERT_EQ(laszip_get_point_pointer(reader, &las_point), 0);

    std::cerr << "[Test] Verifying LASzip roundtrip (first 3 points):" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(3, points.size()); i++) {
      LASPP_ASSERT_EQ(laszip_read_point(reader), 0);
      std::cerr << "[Test]   Point " << i
                << ": scanner_channel=" << static_cast<int>(points[i].scanner_channel)
                << " gps_time=" << std::setprecision(20) << points[i].gps_time
                << " (LASzip read: scanner="
                << static_cast<int>(las_point->extended_scanner_channel)
                << " gps=" << las_point->gps_time << ")" << std::endl;
    }

    LASPP_ASSERT_EQ(laszip_close_reader(reader), 0);
    LASPP_ASSERT_EQ(laszip_destroy(reader), 0);
  }

  // Debug: Read first point with LASzip to verify what was written
  if constexpr (std::is_base_of_v<GPSTime, PointT>) {
    laszip_POINTER reader;
    LASPP_ASSERT_EQ(laszip_create(&reader), 0);
    laszip_BOOL is_compressed = 0;
    LASPP_ASSERT_EQ(laszip_open_reader(reader, temp_file.path().string().c_str(), &is_compressed),
                    0);
    laszip_point* las_point;
    LASPP_ASSERT_EQ(laszip_get_point_pointer(reader, &las_point), 0);
    LASPP_ASSERT_EQ(laszip_read_point(reader), 0);  // Read first point
    std::cerr << "[Test Debug] LASzip first point GPS time=" << std::setprecision(20)
              << las_point->gps_time
              << " (uint64=" << *reinterpret_cast<uint64_t*>(&las_point->gps_time) << ")"
              << std::endl;
    std::cerr << "[Test Debug] Expected first point GPS time=" << std::setprecision(20)
              << static_cast<double>(points[0])
              << " (uint64=" << (static_cast<GPSTime>(points[0]).as_uint64()) << ")" << std::endl;
    LASPP_ASSERT_EQ(laszip_close_reader(reader), 0);
    LASPP_ASSERT_EQ(laszip_destroy(reader), 0);
  }

  std::ifstream laz_file(temp_file.path(), std::ios::binary);
  LASPP_ASSERT(laz_file.is_open());
  LASReader reader(laz_file);

  LASPP_ASSERT_EQ(reader.header().num_points(), points.size());

  std::vector<PointT> decoded(points.size());
  auto decoded_span = reader.read_chunks(std::span<PointT>(decoded), {0, reader.num_chunks()});
  LASPP_ASSERT_EQ(decoded_span.size(), points.size());

  for (size_t i = 0; i < points.size(); ++i) {
    if constexpr (std::is_base_of_v<LASPointFormat6, PointT>) {
      if (decoded_span[i].gps_time != points[i].gps_time) {
        std::cerr << "[Test] Point " << i << " GPS time mismatch:" << std::endl;
        std::cerr << "  Expected: " << std::setprecision(20) << points[i].gps_time << std::endl;
        std::cerr << "  Decoded:  " << std::setprecision(20) << decoded_span[i].gps_time
                  << std::endl;
        std::cerr << "  Delta:    " << (decoded_span[i].gps_time - points[i].gps_time) << std::endl;
      }
    }
    LASPP_ASSERT_EQ(decoded_span[i], points[i], "Point " + std::to_string(i) + " mismatch");
  }
}

}  // namespace

void test_laszip_scanner_channel() {
  // Write a single point with LASzip and read it back with LASzip to verify it preserves
  // scanner_channel
  TempFile temp_file("scanner_channel_test");

  laszip_POINTER writer;
  LASPP_ASSERT_EQ(laszip_create(&writer), 0);
  LASPP_ASSERT_EQ(laszip_request_native_extension(writer, 1), 0);

  laszip_header* header;
  LASPP_ASSERT_EQ(laszip_get_header_pointer(writer, &header), 0);
  header->version_major = 1;
  header->version_minor = 4;
  header->header_size = 375;
  header->offset_to_point_data = 375;
  header->point_data_format = 6;
  header->point_data_record_length = 30;
  header->number_of_point_records = 2;
  LASPP_ASSERT_EQ(laszip_set_header(writer, header), 0);

  LASPP_ASSERT_EQ(laszip_open_writer(writer, temp_file.path().string().c_str(), 1), 0);

  laszip_point* point;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(writer, &point), 0);

  // First point with scanner_channel = 1
  point->X = 0;
  point->Y = 0;
  point->Z = 0;
  point->extended_scanner_channel = 1;
  point->extended_scan_angle = 100;
  std::cerr << "Writing first point: scanner_channel="
            << static_cast<int>(point->extended_scanner_channel) << std::endl;
  LASPP_ASSERT_EQ(laszip_set_point(writer, point), 0);
  LASPP_ASSERT_EQ(laszip_write_point(writer), 0);

  // Second point with scanner_channel = 1
  point->X = 100;
  point->Y = 100;
  point->Z = 100;
  point->extended_scanner_channel = 1;
  point->extended_scan_angle = 200;
  std::cerr << "Writing: scanner_channel=" << static_cast<int>(point->extended_scanner_channel)
            << " scan_angle=" << point->extended_scan_angle << std::endl;
  LASPP_ASSERT_EQ(laszip_set_point(writer, point), 0);
  LASPP_ASSERT_EQ(laszip_write_point(writer), 0);

  LASPP_ASSERT_EQ(laszip_close_writer(writer), 0);
  LASPP_ASSERT_EQ(laszip_destroy(writer), 0);

  // Now read it back with LASzip
  laszip_POINTER reader;
  LASPP_ASSERT_EQ(laszip_create(&reader), 0);
  laszip_BOOL is_compressed;
  LASPP_ASSERT_EQ(laszip_open_reader(reader, temp_file.path().string().c_str(), &is_compressed), 0);

  laszip_point* read_point;
  LASPP_ASSERT_EQ(laszip_get_point_pointer(reader, &read_point), 0);

  LASPP_ASSERT_EQ(laszip_read_point(reader), 0);  // First point
  std::cerr << "Read back first point: scanner_channel="
            << static_cast<int>(read_point->extended_scanner_channel)
            << " scan_angle=" << read_point->extended_scan_angle << std::endl;
  LASPP_ASSERT_EQ(read_point->extended_scanner_channel, 1);
  LASPP_ASSERT_EQ(read_point->extended_scan_angle, 100);

  LASPP_ASSERT_EQ(laszip_read_point(reader), 0);  // Second point
  std::cerr << "Read back second point: scanner_channel="
            << static_cast<int>(read_point->extended_scanner_channel)
            << " scan_angle=" << read_point->extended_scan_angle << std::endl;
  LASPP_ASSERT_EQ(read_point->extended_scanner_channel, 1);
  LASPP_ASSERT_EQ(read_point->extended_scan_angle, 200);

  LASPP_ASSERT_EQ(laszip_close_reader(reader), 0);
  LASPP_ASSERT_EQ(laszip_destroy(reader), 0);
}

void test_gps_encoder_large_delta() {
  // Test if our GPS encoder can handle large deltas like -486k
  std::cerr << "[Test] Testing GPS encoder with large delta..." << std::endl;

  LASPointFormat6 point0{};
  point0.gps_time = -233116.95723205048125;

  LASPointFormat6 point1{};
  point1.x = point0.x;
  point1.y = point0.y;
  point1.z = point0.z;
  point1.intensity = point0.intensity;
  point1.gps_time = -719298.44479162397329;

  std::stringstream ss;
  OutStream out_stream(ss);

  GPSTime11Encoder encoder(GPSTime(point0.gps_time));
  encoder.encode(out_stream, GPSTime(point1.gps_time));

  std::string data = ss.str();
  std::cerr << "[Test] Encoded " << data.size() << " bytes for GPS delta" << std::endl;

  std::istringstream iss(data);
  InStream in_stream(iss);

  GPSTime11Encoder decoder(GPSTime(point0.gps_time));
  GPSTime decoded = decoder.decode(in_stream);

  std::cerr << "[Test] Original: " << std::setprecision(20) << point1.gps_time << std::endl;
  std::cerr << "[Test] Decoded:  " << std::setprecision(20) << static_cast<double>(decoded)
            << std::endl;
  std::cerr << "[Test] Delta:    " << (static_cast<double>(decoded) - point1.gps_time) << std::endl;

  LASPP_ASSERT_EQ(static_cast<double>(decoded), point1.gps_time);
}

int main() {
  run_laszip_roundtrip<LASPointFormat0>(256);
  run_laszip_file_roundtrip<LASPointFormat0>(128, false);
  run_laszip_roundtrip<LASPointFormat1>(256);
  run_laszip_file_roundtrip<LASPointFormat1>(128, false);
  run_laszip_file_roundtrip<LASPointFormat6>(128, true);
  run_laszip_file_roundtrip<LASPointFormat7>(128, true);
  return 0;
}
