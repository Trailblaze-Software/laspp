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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <span>
#include <sstream>
#include <vector>

#include "las_point.hpp"
#include "laz/layered_stream.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/stream.hpp"

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

    std::mt19937 gen(0);
    std::uniform_int_distribution<int32_t> coord_dist(-500000, 500000);
    std::uniform_int_distribution<uint16_t> intensity_dist(0, std::numeric_limits<uint16_t>::max());
    std::uniform_int_distribution<uint16_t> point_source_dist(0,
                                                              std::numeric_limits<uint16_t>::max());
    std::uniform_int_distribution<int16_t> angle_dist(-1800, 1800);
    std::uniform_int_distribution<uint8_t> user_dist(0, std::numeric_limits<uint8_t>::max());
    std::uniform_int_distribution<uint8_t> flag_dist(0, 15);
    std::uniform_int_distribution<uint8_t> scanner_dist(0, 3);
    std::uniform_int_distribution<uint8_t> returns_dist(1, 15);
    std::uniform_int_distribution<uint8_t> bool_dist(0, 1);
    std::uniform_int_distribution<uint8_t> classification_dist(0, 22);
    std::uniform_real_distribution<double> gps_dist(-1e6, 1e6);

    for (size_t i = 1; i < 1000; i++) {
      LASPointFormat6 next = points.back();
      next.scanner_channel = static_cast<uint8_t>(scanner_dist(gen) & 0x3);
      next.number_of_returns = static_cast<uint8_t>(returns_dist(gen) & 0xF);
      uint8_t max_return = next.number_of_returns;
      std::uniform_int_distribution<uint8_t> return_number_dist(1,
                                                                std::max<uint8_t>(1, max_return));
      next.return_number =
          static_cast<uint8_t>(std::min<uint8_t>(return_number_dist(gen), max_return) & 0xF);
      next.classification_flags = static_cast<uint8_t>(flag_dist(gen) & 0x0F);
      next.scan_direction_flag = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      next.edge_of_flight_line = static_cast<uint8_t>(bool_dist(gen) & 0x1);
      next.classification = static_cast<LASClassification>(classification_dist(gen));
      next.user_data = user_dist(gen);
      next.scan_angle = angle_dist(gen);
      next.point_source_id = point_source_dist(gen);
      next.intensity = intensity_dist(gen);
      next.gps_time = gps_dist(gen);
      next.x = coord_dist(gen);
      next.y = coord_dist(gen);
      next.z = coord_dist(gen);
      points.emplace_back(next);
    }

    LayeredOutStreams<LASPointFormat6Encoder::NUM_LAYERS> out_streams;
    LASPointFormat6Encoder encoder(points.front());
    for (size_t i = 1; i < points.size(); i++) {
      encoder.encode(out_streams, points[i]);
    }

    std::stringstream combined_stream = out_streams.combined_stream();
    std::string combined_data = combined_stream.str();
    std::vector<std::byte> buffer(combined_data.size());
    std::memcpy(buffer.data(), combined_data.data(), combined_data.size());

    std::span<std::byte> size_span(buffer.data(),
                                   LASPointFormat6Encoder::NUM_LAYERS * sizeof(uint32_t));
    std::span<std::byte> data_span(buffer.data() + size_span.size(),
                                   buffer.size() - size_span.size());

    LayeredInStreams<LASPointFormat6Encoder::NUM_LAYERS> in_streams(size_span, data_span);

    LASPointFormat6Encoder decoder(points.front());
    for (size_t i = 1; i < points.size(); i++) {
      LASPP_ASSERT_EQ(decoder.decode(in_streams), points[i]);
      LASPP_ASSERT_EQ(decoder.last_value(), points[i]);
    }
  }

  return 0;
}
