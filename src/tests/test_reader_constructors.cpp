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

#include <filesystem>
#include <fstream>
#include <sstream>

#include "las_header.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test LASReader constructor from stream
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    stream.seekg(0);
    LASReader reader(stream);
    LASPP_ASSERT_EQ(reader.header().num_points(), 10u);
    LASPP_ASSERT_EQ(reader.header().point_format(), 0);
  }

  // Test LASReader constructor from stream with file_path (for .lax lookup)
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    stream.seekg(0);
    std::filesystem::path dummy_path("dummy.las");
    LASReader reader(stream, dummy_path);
    LASPP_ASSERT_EQ(reader.header().num_points(), 10u);
    // Should try to look for dummy.lax but won't find it (which is fine)
    LASPP_ASSERT(!reader.has_lastools_spatial_index());
  }

  // Test LASReader constructor from filename (creates temporary file)
  {
    // Create a temporary file
    std::filesystem::path temp_file =
        std::filesystem::temp_directory_path() / "test_las_reader.las";

    {
      std::fstream file_stream(temp_file, std::ios::binary | std::ios::out | std::ios::trunc);
      LASWriter writer(file_stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Test reading from filename
    LASReader reader(temp_file);
    LASPP_ASSERT_EQ(reader.header().num_points(), 10u);
    LASPP_ASSERT_EQ(reader.header().point_format(), 0);

    // Clean up
    std::filesystem::remove(temp_file);
  }

  // Test reading spatial index from .lax file
  {
    // Create a temporary LAS file
    std::filesystem::path temp_file =
        std::filesystem::temp_directory_path() / "test_lax_reader.las";
    std::filesystem::path lax_file = std::filesystem::temp_directory_path() / "test_lax_reader.lax";

    {
      std::fstream file_stream(temp_file, std::ios::binary | std::ios::out | std::ios::trunc);
      LASWriter writer(file_stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Create a .lax file with a spatial index
    {
      std::ofstream lax_stream(lax_file, std::ios::binary);
      QuadtreeSpatialIndex index;
      index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
      index.set_levels(2);
      std::vector<PointInterval> intervals;
      intervals.push_back({0, 9});
      index.add_cell(5, 10, intervals);  // Add a cell at level 2
      index.write(lax_stream);
    }

    // Test reading from filename - should find .lax file
    LASReader reader(temp_file);
    LASPP_ASSERT(reader.has_lastools_spatial_index());
    const auto& spatial_index = reader.lastools_spatial_index();
    LASPP_ASSERT_EQ(spatial_index.num_cells(), 1u);
    LASPP_ASSERT_EQ(spatial_index.quadtree_header().levels, 2u);

    // Clean up
    std::filesystem::remove(temp_file);
    std::filesystem::remove(lax_file);
  }

  // Test that .lax file is ignored if spatial index exists in EVLR
  {
    // Create a temporary LAS file with spatial index in EVLR
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_lax_evlr.las";
    std::filesystem::path lax_file = std::filesystem::temp_directory_path() / "test_lax_evlr.lax";

    {
      std::fstream file_stream(temp_file, std::ios::binary | std::ios::out | std::ios::trunc);
      LASWriter writer(file_stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));

      // Add spatial index to EVLR
      QuadtreeSpatialIndex index;
      index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
      index.set_levels(3);
      std::vector<PointInterval> intervals;
      intervals.push_back({0, 9});
      index.add_cell(21, 10, intervals);  // Add a cell at level 3
      writer.write_lastools_spatial_index(index);
    }

    // Create a different .lax file
    {
      std::ofstream lax_stream(lax_file, std::ios::binary);
      QuadtreeSpatialIndex lax_index;
      lax_index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
      lax_index.set_levels(2);
      std::vector<PointInterval> intervals;
      intervals.push_back({0, 9});
      lax_index.add_cell(5, 10, intervals);
      lax_index.write(lax_stream);
    }

    // Test reading - should use EVLR spatial index, not .lax
    LASReader reader(temp_file);
    LASPP_ASSERT(reader.has_lastools_spatial_index());
    const auto& spatial_index = reader.lastools_spatial_index();
    LASPP_ASSERT_EQ(spatial_index.quadtree_header().levels, 3u);  // From EVLR, not .lax

    // Clean up
    std::filesystem::remove(temp_file);
    std::filesystem::remove(lax_file);
  }

  // Test that corrupted .lax file doesn't crash
  {
    std::filesystem::path temp_file =
        std::filesystem::temp_directory_path() / "test_corrupt_lax.las";
    std::filesystem::path lax_file =
        std::filesystem::temp_directory_path() / "test_corrupt_lax.lax";

    {
      std::fstream file_stream(temp_file, std::ios::binary | std::ios::out | std::ios::trunc);
      LASWriter writer(file_stream, 0, 0);
      std::vector<LASPointFormat0> points(10);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = i * 1000;
        points[i].y = i * 1000;
        points[i].z = i * 100;
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Create a corrupted .lax file
    {
      std::ofstream lax_stream(lax_file, std::ios::binary);
      lax_stream << "CORRUPTED DATA";
    }

    // Should not crash, just not find spatial index
    LASReader reader(temp_file);
    LASPP_ASSERT(!reader.has_lastools_spatial_index());

    // Clean up
    std::filesystem::remove(temp_file);
    std::filesystem::remove(lax_file);
  }

  return 0;
}
