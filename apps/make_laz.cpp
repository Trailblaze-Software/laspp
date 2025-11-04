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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <out_file>" << std::endl;
    return 1;
  }

  std::filesystem::path out_file(argv[1]);
  std::fstream ofs;
  ofs.open(out_file, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  LASPP_ASSERT(ofs.is_open(), "Failed to open ", out_file);

  laspp::LASWriter writer(ofs, 128);

  std::vector<laspp::LASPointFormat0> points;
  for (int i = 0; i < 300; ++i) {
    laspp::LASPointFormat0 point;
    point.x = i * 1000;
    point.y = i * 1000;
    point.z = i * 1000;
    points.push_back(point);
  }

  std::cout << points[2] << std::endl;

  writer.write_points<laspp::LASPointFormat0>(points);

  return 0;
}
