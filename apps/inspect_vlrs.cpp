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

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "las_reader.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"
#include "utilities/printing.hpp"
#include "vlr.hpp"

using namespace laspp;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <laz_file>" << std::endl;
    return 1;
  }

  std::string filename = argv[1];
  std::filesystem::path file_path(filename);
  LASReader reader(file_path);

  std::cout << "=== VLRs ===" << std::endl;
  const auto& vlrs = reader.vlr_headers();
  for (size_t i = 0; i < vlrs.size(); ++i) {
    const auto& vlr = vlrs[i];
    std::cout << "VLR " << i << ":" << std::endl;
    std::cout << indented(vlr, "  ");
    std::cout << "  Is LAZ VLR: " << (vlr.is_laz_vlr() ? "yes" : "no") << std::endl;
    std::cout << "  Is LAStools spatial index VLR: "
              << (vlr.is_lastools_spatial_index_vlr() ? "yes" : "no") << std::endl;
    std::cout << std::endl;
  }

  std::cout << "=== EVLRs ===" << std::endl;
  const auto& evlrs = reader.evlr_headers();
  for (size_t i = 0; i < evlrs.size(); ++i) {
    const auto& evlr = evlrs[i];
    std::cout << "EVLR " << i << ":" << std::endl;
    std::cout << indented(evlr, "  ");
    std::cout << "  Is LAZ VLR: " << (evlr.is_laz_vlr() ? "yes" : "no") << std::endl;
    std::cout << "  Is LAStools spatial index EVLR: "
              << (evlr.is_lastools_spatial_index_evlr() ? "yes" : "no") << std::endl;

    // If it's a LAStools spatial index, show more details
    if (evlr.is_lastools_spatial_index_evlr()) {
      LASPP_ASSERT(reader.has_lastools_spatial_index(),
                   "Failed to load LAStools spatial index from EVLR.");
      std::cout << "Spatial index: " << std::endl;
      const auto& index = reader.lastools_spatial_index();
      const auto& quadtree = index.quadtree_header();

      std::cout << "  Spatial index header:" << std::endl;
      std::cout << indented(quadtree, "    ");
      std::cout << "  Number of cells: " << index.num_cells() << std::endl;

      // Show cell details
      size_t total_intervals = 0;
      for (const auto& [cell_index, cell] : index.cells()) {
        total_intervals += cell.intervals.size();
      }
      std::cout << "  Total intervals: " << total_intervals << std::endl;
      std::cout << "  Average intervals per cell: " << std::fixed << std::setprecision(1)
                << (index.num_cells() > 0 ? static_cast<double>(total_intervals) /
                                                static_cast<double>(index.num_cells())
                                          : 0.0)
                << std::endl;

      // Show first few cells as examples
      std::cout << indented(limited_map(index.cells(), 3), "    ") << std::endl;
    }
    std::cout << std::endl;
  }

  if (reader.has_lastools_spatial_index()) {
    const auto& spatial_index = reader.lastools_spatial_index();
    std::cout << "Loaded LAStools spatial index with " << spatial_index.num_cells() << " cells."
              << std::endl;
    std::cout << "Spatial index details:" << std::endl;
    std::cout << indented(spatial_index, "  ") << std::endl;
  }

  return 0;
}
