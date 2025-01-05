
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

      writer.write_points(std::span<LASPointFormat0>(points));

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 0);
      LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 20);

      const std::vector<LASVLRWithGlobalOffset>& vlrs = reader.vlr_headers();
      LASPP_ASSERT_EQ(vlrs.size(), 1);
      LASPP_ASSERT_EQ(reader.read_vlr_data(vlrs[0]).size(), 0);

      LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), 375 + sizeof(LASVLR));

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
      LASPP_ASSERT_THROWS(writer.write_points(std::span<LASPointFormat3>(points_bad)),
                          std::runtime_error);
      writer.write_points(std::span<LASPointFormat5>(points));

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
      writer.write_points(std::span<LASPointFormat1>(points));

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

      std::vector<LASPointFormat1> points(100);
      reader.read_chunk(std::span<LASPointFormat1>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
        LASPP_ASSERT_EQ(points[i].gps_time.f64, static_cast<double>(i) * 32.0);
      }
    }
  }

  return 0;
}
