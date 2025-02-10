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
#include "laz/point10_encoder.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/stream.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::vector<LASPointFormat6> points;
    points.reserve(1000);
    std::mt19937 gen(0);
    gen.seed(0);
    for (size_t i = points.size(); i < 1000; i++) {
      const LASPointFormat6& prev = points.back();
      LASPointFormat6 next = prev;
      uint8_t changed = gen() % 64;
      if (changed & (1 << 5)) {
        next.bit_byte = static_cast<uint8_t>(gen());
      }
      if (changed & (1 << 4)) {
        next.intensity = static_cast<uint16_t>(gen());
      }
      if (changed & (1 << 3)) {
        next.classification_byte = static_cast<uint8_t>(gen());
      }
      if (changed & (1 << 2)) {
        next.scan_angle_rank = static_cast<uint8_t>(gen());
      }
      if (changed & (1 << 1)) {
        next.user_data = static_cast<uint8_t>(gen());
      }
      if (changed & (1 << 0)) {
        next.point_source_id = static_cast<uint16_t>(gen());
      }
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

    LASPP_ASSERT_EQ(encoded_stream.str().size(), 17377);

    {
      laspp::InStream instream(encoded_stream);
      LASPointFormat6Encoder encoder(points.back());
      for (LASPointFormat6& point : points) {
        LASPP_ASSERT_EQ(encoder.decode(instream), point);
        LASPP_ASSERT_EQ(encoder.last_value(), point);
      }
    }
  }

  return 0;
}
