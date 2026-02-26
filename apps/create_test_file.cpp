/*
 * SPDX-FileCopyrightText: (c) 2026 Trailblaze Software, all rights reserved
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

/*
 * Quick utility to create a test LAZ file for benchmarking
 */

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "las_point.hpp"
#include "las_writer.hpp"

using namespace laspp;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <output.laz> [num_points=1000000]\n";
    return 1;
  }

  std::filesystem::path out_path(argv[1]);
  size_t num_points = argc > 2 ? std::stoull(argv[2]) : 1000000;

  std::cout << "Generating " << num_points << " points to " << out_path << "...\n";

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> coord_dist(-1000000, 1000000);
  std::uniform_int_distribution<uint16_t> intensity_dist(0, 65535);
  std::uniform_int_distribution<int> return_num_dist(1, 5);

  std::vector<LASPointFormat1> points;
  points.reserve(num_points);

  for (size_t i = 0; i < num_points; ++i) {
    LASPointFormat1 pt;
    pt.x = coord_dist(rng);
    pt.y = coord_dist(rng);
    pt.z = coord_dist(rng);
    pt.intensity = intensity_dist(rng);
    pt.bit_byte.return_number = static_cast<uint8_t>(return_num_dist(rng));
    pt.bit_byte.number_of_returns = static_cast<uint8_t>(return_num_dist(rng));
    pt.bit_byte.scan_direction_flag = 0;
    pt.bit_byte.edge_of_flight_line = 0;
    pt.classification_byte.classification = LASClassification::Unclassified;
    pt.classification_byte.synthetic = 0;
    pt.classification_byte.key_point = 0;
    pt.classification_byte.withheld = 0;
    pt.scan_angle_rank = 0;
    pt.user_data = 0;
    pt.point_source_id = 0;
    pt.gps_time.f64 = static_cast<double>(i) * 0.001;
    points.push_back(pt);
  }

  std::fstream ofs(out_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  if (!ofs.is_open()) {
    std::cerr << "Failed to open output file\n";
    return 1;
  }

  // Format 1 with compression (0x81)
  LASWriter writer(ofs, 0x81);
  writer.header().transform().m_scale_factors.x() = 0.01;
  writer.header().transform().m_scale_factors.y() = 0.01;
  writer.header().transform().m_scale_factors.z() = 0.01;
  writer.header().transform().m_offsets.x() = 0.0;
  writer.header().transform().m_offsets.y() = 0.0;
  writer.header().transform().m_offsets.z() = 0.0;

  writer.write_points<LASPointFormat1>(std::span<const LASPointFormat1>(points), 50000);

  std::cout << "Created " << out_path << " (" << std::filesystem::file_size(out_path)
            << " bytes)\n";
  return 0;
}
