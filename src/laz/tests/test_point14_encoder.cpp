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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <span>
#include <sstream>
#include <vector>

#include "las_point.hpp"
#include "laz/layered_stream.hpp"
#include "laz/point14_encoder.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::vector<LASPointFormat6> points;
    points.reserve(1000);
    LASPointFormat6 first_point{};
    first_point.x = 0;
    first_point.y = 0;
    first_point.z = 0;
    first_point.intensity = 0;
    first_point.return_number = 1;
    first_point.number_of_returns = 1;
    first_point.classification_flags = 0;
    first_point.scanner_channel = 0;
    first_point.scan_direction_flag = 0;
    first_point.edge_of_flight_line = 0;
    first_point.classification = LASClassification::OverlapPoints;
    first_point.user_data = 0;
    first_point.scan_angle = 0;
    first_point.point_source_id = 0;
    first_point.gps_time = 0.0;
    points.push_back(first_point);

    std::mt19937_64 gen(0);

    for (size_t i = 1; i < 1000; i++) {
      points.emplace_back(LASPointFormat6::RandomData(gen));
    }

    std::string combined_data;

    {
      LayeredOutStreams<LASPointFormat6Encoder::NUM_LAYERS> out_streams;
      {
        std::unique_ptr<LASPointFormat6Encoder> encoder =
            std::make_unique<LASPointFormat6Encoder>(points.front());
        for (size_t i = 1; i < points.size(); i++) {
          encoder->encode(out_streams, points[i]);
        }
      }

      std::stringstream combined_stream = out_streams.combined_stream();
      combined_data = combined_stream.str();
    }

    LASPP_ASSERT_EQ(combined_data.size(), 30852);

    std::span<std::byte> size_span(reinterpret_cast<std::byte*>(combined_data.data()),
                                   LASPointFormat6Encoder::NUM_LAYERS * sizeof(uint32_t));
    std::span<std::byte> data_span(
        reinterpret_cast<std::byte*>(combined_data.data()) + size_span.size(),
        combined_data.size() - size_span.size());

    LayeredInStreams<LASPointFormat6Encoder::NUM_LAYERS> in_streams(size_span, data_span);

    std::unique_ptr<LASPointFormat6Encoder> decoder =
        std::make_unique<LASPointFormat6Encoder>(points.front());
    LASPP_ASSERT_EQ(decoder->last_value(), points[0]);
    for (size_t i = 1; i < points.size(); i++) {
      LASPP_ASSERT_EQ(decoder->decode(in_streams), points[i]);
      LASPP_ASSERT_EQ(decoder->last_value(), points[i]);
    }
  }

  return 0;
}
