
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

#include <chrono>
#include <cstdlib>
#include <filesystem>

#include "example_custom_las_point.hpp"
#include "las_header.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "vlr.hpp"
using namespace laspp;

class TempFile {
 public:
  explicit TempFile(const std::string& prefix) {
    auto base_dir = std::filesystem::temp_directory_path() / "laspp_tests";
    std::filesystem::create_directories(base_dir);
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = base_dir / (prefix + "_" + std::to_string(timestamp) + ".las");
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    for (uint8_t format : {uint8_t{0}, uint8_t{0 | 128}}) {
      std::stringstream stream;
      {
        LASWriter writer(stream, format, 0);

        std::vector<LASPointFormat0> points;
        points.reserve(100);
        for (size_t i = 0; i < points.capacity(); i++) {
          points.emplace_back();
          points.back().x = static_cast<int32_t>(i);
        }

        writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
        writer.write_wkt("TEST WKT");

        writer.header().transform() = Transform({1, 1, 1}, {0, 0, 0});

        writer.write_points(std::span<const LASPointFormat0>(points));

        LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()),
                            std::runtime_error);
      }

      {
        LASReader reader(stream);
        LASPP_ASSERT_EQ(reader.header().num_points(), 100);
        LASPP_ASSERT_EQ(reader.header().point_format(), format);
        LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 20);
        LASPP_ASSERT_EQ(reader.header().bounds().min_x(), 0);
        LASPP_ASSERT_EQ(reader.header().bounds().max_x(), 99);
        LASPP_ASSERT_EQ(reader.header().bounds().min_y(), 0);
        LASPP_ASSERT_EQ(reader.header().bounds().max_y(), 0);
        LASPP_ASSERT_EQ(reader.header().bounds().min_z(), 0);
        LASPP_ASSERT_EQ(reader.header().bounds().max_z(), 0);

        const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
        bool is_compressed = (format & 128) != 0;
        size_t num_vlrs = is_compressed ? 3 : 2;  // LAZ has extra special VLR
        LASPP_ASSERT_EQ(vlrs.size(), num_vlrs);
        LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

        LASPP_ASSERT_EQ(reader.coordinate_wkt().value(), "TEST WKT");
        LASPP_ASSERT(!reader.math_wkt().has_value());

        // Offset calculation: header + VLR headers + VLR data lengths
        // WKT VLR is at index 1 (9 bytes), LAZ special VLR is at index 2 (if present)
        size_t expected_offset = 375 + num_vlrs * sizeof(LASVLR) + 9;  // 9 = WKT string length
        if (is_compressed) {
          expected_offset += vlrs[2].record_length_after_header;  // LAZ special VLR data
        }
        LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), expected_offset);

        std::vector<LASPointFormat0> points(100);
        reader.read_chunk(std::span<LASPointFormat0>(points), 0);

        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        }
      }
    }
  }

  {
    // Format 5 includes WavePacketData which is not supported in compressed mode
    // So we only test uncompressed format 5
    for (uint8_t format : {uint8_t{5}}) {
      std::stringstream stream;
      {
        LASWriter writer(stream, format, 0);

        std::vector<LASPointFormat3> points_bad;
        std::vector<LASPointFormat5> points;
        points.reserve(100);
        for (size_t i = 0; i < points.capacity(); i++) {
          points.emplace_back();
          points.back().x = static_cast<int32_t>(i);
          points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
        }

        writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
        LASPP_ASSERT_THROWS(writer.write_points(std::span<const LASPointFormat3>(points_bad)),
                            std::runtime_error);
        writer.write_points(std::span<const LASPointFormat5>(points));

        LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()),
                            std::runtime_error);
      }

      {
        LASReader reader(stream);
        LASPP_ASSERT_EQ(reader.header().num_points(), 100);
        LASPP_ASSERT_EQ(reader.header().point_format(), format);
        LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 63);

        const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
        size_t num_vlrs = 1;  // Only uncompressed, so no LAZ special VLR
        LASPP_ASSERT_EQ(vlrs.size(), num_vlrs);
        LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

        size_t expected_offset = 375 + num_vlrs * sizeof(LASVLR);
        LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), expected_offset);

        std::vector<LASPointFormat5> points(100);
        std::vector<LASPointFormat3> points_bad(100);
        LASPP_ASSERT_THROWS(reader.read_chunk(std::span<LASPointFormat3>(points_bad), 0),
                            std::runtime_error);
        reader.read_chunk(std::span<LASPointFormat5>(points), 0);

        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
          LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
        }
      }
    }
  }

  {
    for (uint8_t format : {uint8_t{1}, uint8_t{1 | 128}}) {
      std::stringstream stream;
      {
        LASWriter writer(stream, format, 0);

        std::vector<LASPointFormat1> points;
        points.reserve(100);
        for (size_t i = 0; i < points.capacity(); i++) {
          points.emplace_back();
          points.back().x = static_cast<int32_t>(i);
          points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
        }

        writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
        writer.write_points(std::span<const LASPointFormat1>(points));

        LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()),
                            std::runtime_error);
      }

      {
        LASReader reader(stream);
        LASPP_ASSERT_EQ(reader.header().num_points(), 100);
        LASPP_ASSERT_EQ(reader.header().point_format(), format);
        LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 28);

        const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
        bool is_compressed = (format & 128) != 0;
        size_t num_vlrs = is_compressed ? 2 : 1;  // LAZ has extra special VLR
        LASPP_ASSERT_EQ(vlrs.size(), num_vlrs);
        LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

        size_t expected_offset = 375 + num_vlrs * sizeof(LASVLR);
        if (is_compressed) {
          expected_offset += vlrs[1].record_length_after_header;
        }
        LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), expected_offset);

        LASPP_ASSERT_EQ(reader.num_chunks(), 1);

        std::vector<LASPointFormat1> points(100);
        reader.read_chunk(std::span<LASPointFormat1>(points), 0);

        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
          LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
        }
      }
    }
  }

  {
    for (uint8_t format : {uint8_t{1}, uint8_t{1 | 128}}) {
      std::stringstream stream;
      {
        LASWriter writer(stream, format, 0);

        std::vector<ExampleFullLASPoint> points;
        points.reserve(100);
        for (size_t i = 0; i < points.capacity(); i++) {
          points.emplace_back();
          points.back().position[0] = static_cast<int32_t>(i);
          points.back().gps_time = static_cast<double>(i) * 32.0;
        }

        writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
        writer.write_points(std::span<const ExampleFullLASPoint>(points).subspan(0, 20));
        writer.write_points(std::span<const ExampleFullLASPoint>(points).subspan(20, 61));
        writer.write_points(std::span<const ExampleFullLASPoint>(points).subspan(81, 19));
        LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()),
                            std::runtime_error);

        writer.write_evlr(LASEVLR{.reserved = 23,
                                  .user_id = "hello",
                                  .record_id = 7,
                                  .record_length_after_header = 3,
                                  .description = "description"},
                          std::vector<std::byte>({std::byte(0), std::byte(1), std::byte(2)}));
      }

      {
        bool is_compressed = (format & 128) != 0;
        {
          LASReader reader(stream);
          std::cout << reader.header() << std::endl;
          LASPP_ASSERT_EQ(reader.header().num_points(), 100);
          LASPP_ASSERT_EQ(reader.header().point_format(), format);
          LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 28);

          const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
          size_t num_vlrs = is_compressed ? 2 : 1;
          LASPP_ASSERT_EQ(vlrs.size(), num_vlrs);
          LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

          size_t expected_offset = 375 + num_vlrs * sizeof(LASVLR);
          if (is_compressed) {
            expected_offset += vlrs[1].record_length_after_header;
          }
          LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), expected_offset);

          LASPP_ASSERT_EQ(reader.header().EVLR_count(), 1);
          const std::vector<LASEVLRWithGlobalOffset>& evlrs = reader.evlr_headers();
          LASPP_ASSERT_EQ(evlrs.size(), 1);
          LASPP_ASSERT_EQ(reader.read_evlr_data(evlrs[0]),
                          std::vector<std::byte>({std::byte(0), std::byte(1), std::byte(2)}));

          std::vector<ExampleFullLASPoint> points(100);
          if (is_compressed) {
            LASPP_ASSERT_EQ(reader.num_chunks(), 3);
            reader.read_chunk(std::span<ExampleFullLASPoint>(points).subspan(0, 20), 0);
            reader.read_chunk(std::span<ExampleFullLASPoint>(points).subspan(20, 61), 1);
            reader.read_chunk(std::span<ExampleFullLASPoint>(points).subspan(81, 19), 2);
          } else {
            LASPP_ASSERT_EQ(reader.num_chunks(), 1);
            reader.read_chunk(std::span<ExampleFullLASPoint>(points), 0);
          }

          for (size_t i = 0; i < points.size(); i++) {
            LASPP_ASSERT_EQ(points[i].position[0], static_cast<int32_t>(i));
            LASPP_ASSERT_EQ(points[i].gps_time, static_cast<double>(i) * 32.0);
          }
        }
        stream.seekg(0);
        {
          LASReader reader(stream);

          std::vector<LASPointFormat1> points(100);
          reader.read_chunks<LASPointFormat1>(points, {0, reader.num_chunks()});
          std::vector<LASPointFormat0> points_0(100);
          if (!is_compressed) {
            LASPP_ASSERT_THROWS(
                reader.read_chunks<LASPointFormat0>(points_0, {0, reader.num_chunks()}),
                std::runtime_error);
          }

          for (size_t i = 0; i < points.size(); i++) {
            LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
            LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
          }
        }
      }
    }
  }

  // Test both memory-mapped and stream-based I/O paths
  {
    TempFile temp_file("test_io_paths");
    {
      // Write a test file
      std::fstream ofs(temp_file.path(),
                       std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
      LASWriter writer(ofs, 1);

      std::vector<LASPointFormat1> points;
      points.reserve(50);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
        points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
      }

      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
      writer.write_points(std::span<const LASPointFormat1>(points));
    }

    // Test memory-mapped path (file path constructor)
    {
      LASReader reader(temp_file.path());
      LASPP_ASSERT(reader.is_using_memory_mapping(), "Reader should be using memory mapping");
      LASPP_ASSERT_EQ(reader.header().num_points(), 50);
      LASPP_ASSERT_EQ(reader.header().point_format(), 1);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 1);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      std::vector<LASPointFormat1> points(50);
      reader.read_chunk(std::span<LASPointFormat1>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }

    // Test stream-based path (istream constructor)
    {
      std::ifstream ifs(temp_file.path(), std::ios::binary);
      LASReader reader(ifs);
      LASPP_ASSERT(!reader.is_using_memory_mapping(), "Reader should be using stream I/O");
      LASPP_ASSERT_EQ(reader.header().num_points(), 50);
      LASPP_ASSERT_EQ(reader.header().point_format(), 1);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 1);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      std::vector<LASPointFormat1> points(50);
      reader.read_chunk(std::span<LASPointFormat1>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }
  }

  // Test both I/O paths with EVLR
  {
    TempFile temp_file("test_io_paths_evlr");
    {
      // Write a test file with EVLR
      std::fstream ofs(temp_file.path(),
                       std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
      LASWriter writer(ofs, 1);

      std::vector<LASPointFormat1> points;
      points.reserve(30);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
      }

      writer.write_points(std::span<const LASPointFormat1>(points));
      writer.write_evlr(LASEVLR{.reserved = 23,
                                .user_id = "test",
                                .record_id = 7,
                                .record_length_after_header = 5,
                                .description = "test evlr"},
                        std::vector<std::byte>({std::byte(10), std::byte(20), std::byte(30),
                                                std::byte(40), std::byte(50)}));
    }

    // Test memory-mapped path with EVLR
    {
      LASReader reader(temp_file.path());
      LASPP_ASSERT(reader.is_using_memory_mapping(), "Reader should be using memory mapping");
      LASPP_ASSERT_EQ(reader.header().num_points(), 30);

      const std::vector<LASEVLRWithGlobalOffset>& evlrs = reader.evlr_headers();
      LASPP_ASSERT_EQ(evlrs.size(), 1);
      std::vector<std::byte> evlr_data = reader.read_evlr_data(evlrs[0]);
      LASPP_ASSERT_EQ(evlr_data.size(), 5);
      LASPP_ASSERT_EQ(evlr_data[0], std::byte(10));
      LASPP_ASSERT_EQ(evlr_data[4], std::byte(50));
    }

    // Test stream-based path with EVLR
    {
      std::ifstream ifs(temp_file.path(), std::ios::binary);
      LASReader reader(ifs);
      LASPP_ASSERT(!reader.is_using_memory_mapping(), "Reader should be using stream I/O");
      LASPP_ASSERT_EQ(reader.header().num_points(), 30);

      const std::vector<LASEVLRWithGlobalOffset>& evlrs = reader.evlr_headers();
      LASPP_ASSERT_EQ(evlrs.size(), 1);
      std::vector<std::byte> evlr_data = reader.read_evlr_data(evlrs[0]);
      LASPP_ASSERT_EQ(evlr_data.size(), 5);
      LASPP_ASSERT_EQ(evlr_data[0], std::byte(10));
      LASPP_ASSERT_EQ(evlr_data[4], std::byte(50));
    }
  }

  // Test both I/O paths with compressed LAZ
  {
    TempFile temp_file("test_io_paths_laz");
    {
      // Write a compressed LAZ file
      std::fstream ofs(temp_file.path(),
                       std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
      LASWriter writer(ofs, 1 | 128);  // Format 1 with compression

      std::vector<LASPointFormat1> points;
      points.reserve(100);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
        points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
      }

      writer.write_points(std::span<const LASPointFormat1>(points));
    }

    // Test memory-mapped path with compressed LAZ
    {
      LASReader reader(temp_file.path());
      LASPP_ASSERT(reader.is_using_memory_mapping(), "Reader should be using memory mapping");
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 129);  // Format 1 with compression
      LASPP_ASSERT_GE(reader.num_chunks(), 1);               // Should have at least one chunk

      std::vector<LASPointFormat1> points(100);
      reader.read_chunks(std::span<LASPointFormat1>(points), {0, reader.num_chunks()});

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }

    // Test stream-based path with compressed LAZ
    {
      std::ifstream ifs(temp_file.path(), std::ios::binary);
      LASReader reader(ifs);
      LASPP_ASSERT(!reader.is_using_memory_mapping(), "Reader should be using stream I/O");
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 129);  // Format 1 with compression
      LASPP_ASSERT_GE(reader.num_chunks(), 1);               // Should have at least one chunk

      std::vector<LASPointFormat1> points(100);
      reader.read_chunks(std::span<LASPointFormat1>(points), {0, reader.num_chunks()});

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }
  }

  // Test LASPP_DISABLE_MMAP environment variable
  {
    TempFile temp_file("test_disable_mmap");
    {
      // Write a test file
      std::fstream ofs(temp_file.path(),
                       std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
      LASWriter writer(ofs, 1);

      std::vector<LASPointFormat1> points;
      points.reserve(25);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
        points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
      }

      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
      writer.write_points(std::span<const LASPointFormat1>(points));
    }

    // Test with LASPP_DISABLE_MMAP set to "1" (should force stream I/O)
    {
#ifdef _WIN32
      _putenv_s("LASPP_DISABLE_MMAP", "1");
#else
      setenv("LASPP_DISABLE_MMAP", "1", 1);
#endif

      LASReader reader(temp_file.path());
      LASPP_ASSERT(!reader.is_using_memory_mapping(),
                   "Reader should be using stream I/O when LASPP_DISABLE_MMAP=1");
      LASPP_ASSERT_EQ(reader.header().num_points(), 25);
      LASPP_ASSERT_EQ(reader.header().point_format(), 1);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 1);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      std::vector<LASPointFormat1> points(25);
      reader.read_chunk(std::span<LASPointFormat1>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }

      // Clean up environment variable
#ifdef _WIN32
      _putenv_s("LASPP_DISABLE_MMAP", "");
#else
      unsetenv("LASPP_DISABLE_MMAP");
#endif
    }

    // Test with LASPP_DISABLE_MMAP set to "0" (should use memory mapping)
    {
#ifdef _WIN32
      _putenv_s("LASPP_DISABLE_MMAP", "0");
#else
      setenv("LASPP_DISABLE_MMAP", "0", 1);
#endif

      LASReader reader(temp_file.path());
      LASPP_ASSERT(reader.is_using_memory_mapping(),
                   "Reader should be using memory mapping when LASPP_DISABLE_MMAP=0");
      LASPP_ASSERT_EQ(reader.header().num_points(), 25);

      // Clean up environment variable
#ifdef _WIN32
      _putenv_s("LASPP_DISABLE_MMAP", "");
#else
      unsetenv("LASPP_DISABLE_MMAP");
#endif
    }

    // Test with LASPP_DISABLE_MMAP unset (should use memory mapping by default)
    {
#ifdef _WIN32
      _putenv_s("LASPP_DISABLE_MMAP", "");
#else
      unsetenv("LASPP_DISABLE_MMAP");
#endif

      LASReader reader(temp_file.path());
      LASPP_ASSERT(reader.is_using_memory_mapping(),
                   "Reader should be using memory mapping by default");
      LASPP_ASSERT_EQ(reader.header().num_points(), 25);
    }
  }

  return 0;
}
