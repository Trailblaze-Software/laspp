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

#include <cstdint>
#include <cstring>
#include <random>
#include <sstream>

#include "las_point.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/stream.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::vector<LASPointFormat6> points;
    LASPointFormat6 first_point;
    first_point.x = 0;
    first_point.y = 0;
    first_point.z = 0;
    first_point.intensity = 0;
    first_point.return_number = 0;
    first_point.number_of_returns = 0;
    first_point.scan_direction_flag = 0;
    first_point.edge_of_flight_line = 0;
    first_point.classification = LASClassification::OverlapPoints;
    first_point.scan_angle = 0;
    first_point.user_data = 0;
    first_point.point_source_id = 0;
    first_point.gps_time = 0;
    points.push_back(first_point);
    points.reserve(1000);
    std::mt19937 gen(0);
    gen.seed(0);
    for (size_t i = points.size(); i < 1000; i++) {
      const LASPointFormat6& prev = points.back();
      LASPointFormat6 next = prev;
      // uint8_t changed = gen() % 128;
      next.x = static_cast<int32_t>(gen());
      next.y = static_cast<int32_t>(gen());
      next.z = static_cast<int32_t>(gen());
      points.emplace_back(next);
    }

    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      LASPointFormat6Encoder encoder(points.back());
      for (LASPointFormat6& point : points) {
        encoder.encode(ostream, point);
      }
    }

    // LASPP_ASSERT_EQ(encoded_stream.str().size(), 17377);
    //
    //{
    // laspp::InStream instream(encoded_stream);
    // LASPointFormat6Encoder encoder(points.back());
    // for (LASPointFormat6& point : points) {
    // LASPP_ASSERT_EQ(encoder.decode(instream), point);
    // LASPP_ASSERT_EQ(encoder.last_value(), point);
    //}
    //}
  }

  return 0;
}
