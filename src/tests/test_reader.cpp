
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
      writer.write_points(std::span<LASPointFormat0>(points));

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
      LASPP_ASSERT_EQ(reader.header().num_points(), 100);
      LASPP_ASSERT_EQ(reader.header().point_format(), 0);
      LASPP_ASSERT_EQ(reader.header().point_data_record_length(), 20);
      LASPP_ASSERT_EQ(reader.header().offset_to_point_data(), 375);

      std::vector<LASPointFormat0> points(100);
      reader.read_chunk(std::span<LASPointFormat0>(points), 0);

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(points[i].x, static_cast<int32_t>(i));
      }
    }
  }

  return 0;
}
