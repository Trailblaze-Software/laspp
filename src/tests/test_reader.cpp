
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

#include "example_custom_las_point.hpp"
#include "las_header.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "vlr.hpp"
using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);

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

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 0);
      LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 20);
      LASPP_ASSERT_EQ(reader.header().bounds().min_x(), 0);
      LASPP_ASSERT_EQ(reader.header().bounds().max_x(), 99);
      LASPP_ASSERT_EQ(reader.header().bounds().min_y(), 0);
      LASPP_ASSERT_EQ(reader.header().bounds().max_y(), 0);
      LASPP_ASSERT_EQ(reader.header().bounds().min_z(), 0);
      LASPP_ASSERT_EQ(reader.header().bounds().max_z(), 0);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 2);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      LASPP_ASSERT_EQ(reader.coordinate_wkt().value(), "TEST WKT");
      LASPP_ASSERT(!reader.math_wkt().has_value());

      LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), 375 + 2 * sizeof(LASVLR) + 9);

      std::vector<LASPointFormat0> points(100);
      reader.read_chunk(std::span<LASPointFormat0>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
      }
    }
  }

  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 5, 0);

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

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 5);
      LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 63);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 1);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), 375 + sizeof(LASVLR));

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

  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 1 | 128, 0);

      std::vector<LASPointFormat1> points;
      points.reserve(100);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
        points.back().gps_time.f64 = static_cast<double>(i) * 32.0;
      }

      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));
      writer.write_points(std::span<const LASPointFormat1>(points));

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 129);
      LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 28);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 2);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      LASPP_ASSERT_EQ(reader.header().offset_to_point_data(),
                      375 + 2 * sizeof(LASVLR) + vlrs[1].record_length_after_header);

      LASPP_ASSERT_EQ(reader.num_chunks(), 1);

      std::vector<LASPointFormat1> points(100);
      reader.read_chunk(std::span<LASPointFormat1>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }
  }

  {
    std::stringstream las_stream;
    std::stringstream laz_stream;
    {
      LASWriter las_writer(las_stream, 1, 0);
      LASWriter laz_writer(laz_stream, 1 | 128, 0);

      std::vector<ExampleFullLASPoint> points;
      points.reserve(100);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().position[0] = static_cast<int32_t>(i);
        points.back().gps_time = static_cast<double>(i) * 32.0;
      }

      for (LASWriter* writer : {&las_writer, &laz_writer}) {
        writer->write_vlr(LASVLR(), std::vector<std::byte>(0));
        writer->write_points(std::span<const ExampleFullLASPoint>(points).subspan(0, 20));
        writer->write_points(std::span<const ExampleFullLASPoint>(points).subspan(20, 61));
        writer->write_points(std::span<const ExampleFullLASPoint>(points).subspan(81, 19));
        LASPP_ASSERT_THROWS(writer->write_vlr(LASVLR(), std::vector<std::byte>()),
                            std::runtime_error);

        writer->write_evlr(LASEVLR{.reserved = 23,
                                   .user_id = "hello",
                                   .record_id = 7,
                                   .record_length_after_header = 3,
                                   .description = "description"},
                           std::vector<std::byte>({std::byte(0), std::byte(1), std::byte(2)}));
      }
    }

    {
      for (std::stringstream* stream : {&las_stream, &laz_stream}) {
        {
          LASReader reader(*stream);
          std::cout << reader.header() << std::endl;
          LASPP_ASSERT_EQ(reader.header().num_points(), 100);
          LASPP_ASSERT_EQ(reader.header().point_format(),
                          stream == &las_stream ? uint8_t{1} : uint8_t{129});
          LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 28);

          const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
          size_t num_vlrs = stream == &las_stream ? 1 : 2;
          LASPP_ASSERT_EQ(vlrs.size(), num_vlrs);
          LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

          LASPP_ASSERT_EQ(reader.header().offset_to_point_data(),
                          375 + num_vlrs * sizeof(LASVLR) +
                              (stream == &las_stream ? 0u : vlrs[1].record_length_after_header));

          LASPP_ASSERT_EQ(reader.header().EVLR_count(), 1);
          const std::vector<LASEVLRWithGlobalOffset>& evlrs = reader.evlr_headers();
          LASPP_ASSERT_EQ(evlrs.size(), 1);
          LASPP_ASSERT_EQ(reader.read_evlr_data(evlrs[0]),
                          std::vector<std::byte>({std::byte(0), std::byte(1), std::byte(2)}));

          std::vector<ExampleFullLASPoint> points(100);
          if (stream == &laz_stream) {
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
        stream->seekg(0);
        {
          LASReader reader(*stream);

          std::vector<LASPointFormat1> points(100);
          reader.read_chunks<LASPointFormat1>(points, {0, reader.num_chunks()});
          std::vector<LASPointFormat0> points_0(100);
          if (stream == &las_stream) {
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

  return 0;
}
