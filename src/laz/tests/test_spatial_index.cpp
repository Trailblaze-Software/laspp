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
#include <sstream>

#include "las_header.hpp"
#include "las_point.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test default constructor
  {
    QuadtreeSpatialIndex index;
    const auto& header = index.quadtree_header();

    LASPP_ASSERT(memcmp(header.spatial_signature, "LASS", 4) == 0);
    LASPP_ASSERT_EQ(header.type, 0u);
    LASPP_ASSERT(memcmp(header.quadtree_signature, "LASQ", 4) == 0);
    LASPP_ASSERT_EQ(header.version, 0u);
    LASPP_ASSERT_EQ(header.levels, 0u);
    LASPP_ASSERT_EQ(header.level_index, 0u);
    LASPP_ASSERT_EQ(header.implicit_levels, 0u);
    LASPP_ASSERT_EQ(header.min_x, 0.0f);
    LASPP_ASSERT_EQ(header.max_x, 0.0f);
    LASPP_ASSERT_EQ(header.min_y, 0.0f);
    LASPP_ASSERT_EQ(header.max_y, 0.0f);
    LASPP_ASSERT_EQ(index.num_cells(), 0);
  }

  // Test setting bounds and levels
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(10.0f, 20.0f, 30.0f, 40.0f);
    index.set_levels(5);

    const auto& header = index.quadtree_header();
    LASPP_ASSERT_EQ(header.min_x, 10.0f);
    LASPP_ASSERT_EQ(header.min_y, 20.0f);
    LASPP_ASSERT_EQ(header.max_x, 30.0f);
    LASPP_ASSERT_EQ(header.max_y, 40.0f);
    LASPP_ASSERT_EQ(header.levels, 5u);
  }

  // Test adding cells
  {
    QuadtreeSpatialIndex index;

    // Add a cell with one interval
    std::vector<PointInterval> intervals1;
    intervals1.push_back({0, 99});
    index.add_cell(0, 100, intervals1);

    LASPP_ASSERT_EQ(index.num_cells(), 1);
    const auto& cells = index.cells();
    LASPP_ASSERT(cells.find(0) != cells.end());
    const auto& cell0 = cells.at(0);
    LASPP_ASSERT_EQ(cell0.cell_index, 0);
    LASPP_ASSERT_EQ(cell0.number_points, 100u);
    LASPP_ASSERT_EQ(cell0.intervals.size(), 1);
    LASPP_ASSERT_EQ(cell0.intervals[0].start, 0u);
    LASPP_ASSERT_EQ(cell0.intervals[0].end, 99u);

    // Add another cell with multiple intervals
    std::vector<PointInterval> intervals2;
    intervals2.push_back({100, 149});
    intervals2.push_back({200, 249});
    index.add_cell(1, 100, intervals2);

    LASPP_ASSERT_EQ(index.num_cells(), 2);
    const auto& cell1 = cells.at(1);
    LASPP_ASSERT_EQ(cell1.cell_index, 1);
    LASPP_ASSERT_EQ(cell1.number_points, 100u);
    LASPP_ASSERT_EQ(cell1.intervals.size(), 2);
    LASPP_ASSERT_EQ(cell1.intervals[0].start, 100u);
    LASPP_ASSERT_EQ(cell1.intervals[0].end, 149u);
    LASPP_ASSERT_EQ(cell1.intervals[1].start, 200u);
    LASPP_ASSERT_EQ(cell1.intervals[1].end, 249u);
  }

  // Test write and read round-trip
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(1.0f, 2.0f, 3.0f, 4.0f);
    index.set_levels(3);

    // Add multiple cells
    std::vector<PointInterval> intervals1;
    intervals1.push_back({0, 49});
    index.add_cell(0, 50, intervals1);

    std::vector<PointInterval> intervals2;
    intervals2.push_back({50, 99});
    intervals2.push_back({150, 199});
    index.add_cell(1, 100, intervals2);

    std::vector<PointInterval> intervals3;
    intervals3.push_back({100, 149});
    index.add_cell(2, 50, intervals3);

    // Write to stream
    std::stringstream ss;
    index.write(ss);

    // Read back
    QuadtreeSpatialIndex read_index(ss);

    // Verify header
    const auto& read_header = read_index.quadtree_header();
    LASPP_ASSERT(memcmp(read_header.spatial_signature, "LASS", 4) == 0);
    LASPP_ASSERT_EQ(read_header.type, 0u);
    LASPP_ASSERT(memcmp(read_header.quadtree_signature, "LASQ", 4) == 0);
    LASPP_ASSERT_EQ(read_header.version, 0u);
    LASPP_ASSERT_EQ(read_header.levels, 3u);
    LASPP_ASSERT_EQ(read_header.level_index, 0u);
    LASPP_ASSERT_EQ(read_header.implicit_levels, 0u);
    LASPP_ASSERT_EQ(read_header.min_x, 1.0f);
    LASPP_ASSERT_EQ(read_header.min_y, 2.0f);
    LASPP_ASSERT_EQ(read_header.max_x, 3.0f);
    LASPP_ASSERT_EQ(read_header.max_y, 4.0f);

    // Verify cells
    LASPP_ASSERT_EQ(read_index.num_cells(), 3);
    const auto& read_cells = read_index.cells();

    LASPP_ASSERT(read_cells.find(0) != read_cells.end());
    const auto& read_cell0 = read_cells.at(0);
    LASPP_ASSERT_EQ(read_cell0.cell_index, 0);
    LASPP_ASSERT_EQ(read_cell0.number_points, 50u);
    LASPP_ASSERT_EQ(read_cell0.intervals.size(), 1);
    LASPP_ASSERT_EQ(read_cell0.intervals[0].start, 0u);
    LASPP_ASSERT_EQ(read_cell0.intervals[0].end, 49u);

    LASPP_ASSERT(read_cells.find(1) != read_cells.end());
    const auto& read_cell1 = read_cells.at(1);
    LASPP_ASSERT_EQ(read_cell1.cell_index, 1);
    LASPP_ASSERT_EQ(read_cell1.number_points, 100u);
    LASPP_ASSERT_EQ(read_cell1.intervals.size(), 2);
    LASPP_ASSERT_EQ(read_cell1.intervals[0].start, 50u);
    LASPP_ASSERT_EQ(read_cell1.intervals[0].end, 99u);
    LASPP_ASSERT_EQ(read_cell1.intervals[1].start, 150u);
    LASPP_ASSERT_EQ(read_cell1.intervals[1].end, 199u);

    LASPP_ASSERT(read_cells.find(2) != read_cells.end());
    const auto& read_cell2 = read_cells.at(2);
    LASPP_ASSERT_EQ(read_cell2.cell_index, 2);
    LASPP_ASSERT_EQ(read_cell2.number_points, 50u);
    LASPP_ASSERT_EQ(read_cell2.intervals.size(), 1);
    LASPP_ASSERT_EQ(read_cell2.intervals[0].start, 100u);
    LASPP_ASSERT_EQ(read_cell2.intervals[0].end, 149u);
  }

  // Test with many cells
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(4);

    // Add 10 cells
    for (int32_t i = 0; i < 10; ++i) {
      std::vector<PointInterval> intervals;
      intervals.push_back({static_cast<uint32_t>(i * 100), static_cast<uint32_t>(i * 100 + 99)});
      index.add_cell(i, 100, intervals);
    }

    LASPP_ASSERT_EQ(index.num_cells(), 10);

    // Write and read back
    std::stringstream ss;
    index.write(ss);
    QuadtreeSpatialIndex read_index(ss);

    LASPP_ASSERT_EQ(read_index.num_cells(), 10);
    const auto& read_cells = read_index.cells();

    for (int32_t i = 0; i < 10; ++i) {
      LASPP_ASSERT(read_cells.find(i) != read_cells.end());
      const auto& cell = read_cells.at(i);
      LASPP_ASSERT_EQ(cell.cell_index, i);
      LASPP_ASSERT_EQ(cell.number_points, 100u);
      LASPP_ASSERT_EQ(cell.intervals.size(), 1);
      LASPP_ASSERT_EQ(cell.intervals[0].start, static_cast<uint32_t>(i * 100));
      LASPP_ASSERT_EQ(cell.intervals[0].end, static_cast<uint32_t>(i * 100 + 99));
    }
  }

  // Test with cells that have many intervals
  {
    QuadtreeSpatialIndex index;

    // Add a cell with 5 intervals
    std::vector<PointInterval> intervals;
    intervals.push_back({0, 9});
    intervals.push_back({20, 29});
    intervals.push_back({40, 49});
    intervals.push_back({60, 69});
    intervals.push_back({80, 89});
    index.add_cell(42, 50, intervals);

    LASPP_ASSERT_EQ(index.num_cells(), 1);
    const auto& cells = index.cells();
    const auto& cell = cells.at(42);
    LASPP_ASSERT_EQ(cell.intervals.size(), 5);

    // Write and read back
    std::stringstream ss;
    index.write(ss);
    QuadtreeSpatialIndex read_index(ss);

    const auto& read_cells = read_index.cells();
    const auto& read_cell = read_cells.at(42);
    LASPP_ASSERT_EQ(read_cell.intervals.size(), 5);
    LASPP_ASSERT_EQ(read_cell.intervals[0].start, 0u);
    LASPP_ASSERT_EQ(read_cell.intervals[0].end, 9u);
    LASPP_ASSERT_EQ(read_cell.intervals[4].start, 80u);
    LASPP_ASSERT_EQ(read_cell.intervals[4].end, 89u);
  }

  // Test with negative cell indices
  {
    QuadtreeSpatialIndex index;

    std::vector<PointInterval> intervals;
    intervals.push_back({0, 99});
    index.add_cell(-1, 100, intervals);
    index.add_cell(-2, 100, intervals);

    LASPP_ASSERT_EQ(index.num_cells(), 2);

    // Write and read back
    std::stringstream ss;
    index.write(ss);
    QuadtreeSpatialIndex read_index(ss);

    LASPP_ASSERT_EQ(read_index.num_cells(), 2);
    const auto& read_cells = read_index.cells();
    LASPP_ASSERT(read_cells.find(-1) != read_cells.end());
    LASPP_ASSERT(read_cells.find(-2) != read_cells.end());
  }

  // Test with zero intervals (edge case)
  {
    QuadtreeSpatialIndex index;

    std::vector<PointInterval> intervals;  // Empty
    index.add_cell(0, 0, intervals);

    LASPP_ASSERT_EQ(index.num_cells(), 1);
    const auto& cells = index.cells();
    const auto& cell = cells.at(0);
    LASPP_ASSERT_EQ(cell.number_points, 0u);
    LASPP_ASSERT_EQ(cell.intervals.size(), 0);

    // Write and read back
    std::stringstream ss;
    index.write(ss);
    QuadtreeSpatialIndex read_index(ss);

    const auto& read_cells = read_index.cells();
    const auto& read_cell = read_cells.at(0);
    LASPP_ASSERT_EQ(read_cell.number_points, 0u);
    LASPP_ASSERT_EQ(read_cell.intervals.size(), 0);
  }

  // Test with large values
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(-1000.0f, -2000.0f, 3000.0f, 4000.0f);
    index.set_levels(20);

    std::vector<PointInterval> intervals;
    intervals.push_back({0, 999999});
    index.add_cell(12345, 1000000, intervals);

    // Write and read back
    std::stringstream ss;
    index.write(ss);
    QuadtreeSpatialIndex read_index(ss);

    const auto& read_header = read_index.quadtree_header();
    LASPP_ASSERT_EQ(read_header.min_x, -1000.0f);
    LASPP_ASSERT_EQ(read_header.min_y, -2000.0f);
    LASPP_ASSERT_EQ(read_header.max_x, 3000.0f);
    LASPP_ASSERT_EQ(read_header.max_y, 4000.0f);
    LASPP_ASSERT_EQ(read_header.levels, 20u);

    const auto& read_cells = read_index.cells();
    const auto& read_cell = read_cells.at(12345);
    LASPP_ASSERT_EQ(read_cell.number_points, 1000000u);
    LASPP_ASSERT_EQ(read_cell.intervals[0].start, 0u);
    LASPP_ASSERT_EQ(read_cell.intervals[0].end, 999999u);
  }

  // Test get_cell_index
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(2);

    // Test cell index calculation for different positions
    // With 2 levels, level_offset[2] = 5, so cells are at indices 5-20
    // Cell 0 (path): [0, 25) x [0, 25) -> index 5 (5 + 0)
    int32_t cell0 = index.get_cell_index(10.0, 10.0);
    LASPP_ASSERT_EQ(cell0, 5);

    // Cell 1 (path): [25, 50) x [0, 25) -> index 6 (5 + 1)
    int32_t cell1 = index.get_cell_index(30.0, 10.0);
    LASPP_ASSERT_EQ(cell1, 6);

    // Cell 2 (path): [0, 25) x [25, 50) -> index 7 (5 + 2)
    int32_t cell2 = index.get_cell_index(10.0, 30.0);
    LASPP_ASSERT_EQ(cell2, 7);

    // Cell 3 (path): [25, 50) x [25, 50) -> index 8 (5 + 3)
    int32_t cell3 = index.get_cell_index(30.0, 30.0);
    LASPP_ASSERT_EQ(cell3, 8);

    // Test edge cases
    int32_t cell_edge_min = index.get_cell_index(0.0, 0.0);
    LASPP_ASSERT_EQ(cell_edge_min, 5);  // level_offset[2] = 5

    int32_t cell_edge_max = index.get_cell_index(99.99, 99.99);
    LASPP_ASSERT_GE(cell_edge_max, 5);
    LASPP_ASSERT_LT(cell_edge_max, 21);  // level_offset[2] + 16 = 21

    // Test with 0 levels (should return 0)
    QuadtreeSpatialIndex index_zero;
    index_zero.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index_zero.set_levels(0);
    int32_t cell_zero = index_zero.get_cell_index(50.0, 50.0);
    LASPP_ASSERT_EQ(cell_zero, 0);

    // Test with invalid bounds (dx <= 0 or dy <= 0)
    QuadtreeSpatialIndex index_invalid;
    index_invalid.set_bounds(100.0f, 100.0f, 100.0f, 100.0f);
    index_invalid.set_levels(2);
    int32_t cell_invalid = index_invalid.get_cell_index(50.0, 50.0);
    LASPP_ASSERT_EQ(cell_invalid, 0);
  }

  // Test get_cell_bounds
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(2);

    // With 2 levels, level_offset[2] = 5, so cells are at indices 5-20
    // Test cell 0 (path) bounds: [0, 25) x [0, 25) -> index 5
    Bound2D cell0_bounds = index.get_cell_bounds(5);
    LASPP_ASSERT_EQ(cell0_bounds.min_x(), 0.0);
    LASPP_ASSERT_EQ(cell0_bounds.min_y(), 0.0);
    LASPP_ASSERT_EQ(cell0_bounds.max_x(), 25.0);
    LASPP_ASSERT_EQ(cell0_bounds.max_y(), 25.0);

    // Test cell 1 (path) bounds: [25, 50) x [0, 25) -> index 6
    Bound2D cell1_bounds = index.get_cell_bounds(6);
    LASPP_ASSERT_EQ(cell1_bounds.min_x(), 25.0);
    LASPP_ASSERT_EQ(cell1_bounds.min_y(), 0.0);
    LASPP_ASSERT_EQ(cell1_bounds.max_x(), 50.0);
    LASPP_ASSERT_EQ(cell1_bounds.max_y(), 25.0);

    // Test cell 2 (path) bounds: [0, 25) x [25, 50) -> index 7
    Bound2D cell2_bounds = index.get_cell_bounds(7);
    LASPP_ASSERT_EQ(cell2_bounds.min_x(), 0.0);
    LASPP_ASSERT_EQ(cell2_bounds.min_y(), 25.0);
    LASPP_ASSERT_EQ(cell2_bounds.max_x(), 25.0);
    LASPP_ASSERT_EQ(cell2_bounds.max_y(), 50.0);

    // Test cell 3 (path) bounds: [25, 50) x [25, 50) -> index 8
    Bound2D cell3_bounds = index.get_cell_bounds(8);
    LASPP_ASSERT_EQ(cell3_bounds.min_x(), 25.0);
    LASPP_ASSERT_EQ(cell3_bounds.min_y(), 25.0);
    LASPP_ASSERT_EQ(cell3_bounds.max_x(), 50.0);
    LASPP_ASSERT_EQ(cell3_bounds.max_y(), 50.0);

    // Test with 0 levels (should return full bounds)
    QuadtreeSpatialIndex index_zero;
    index_zero.set_bounds(10.0f, 20.0f, 30.0f, 40.0f);
    index_zero.set_levels(0);
    Bound2D zero_bounds = index_zero.get_cell_bounds(0);
    LASPP_ASSERT_EQ(zero_bounds.min_x(), 10.0);
    LASPP_ASSERT_EQ(zero_bounds.min_y(), 20.0);
    LASPP_ASSERT_EQ(zero_bounds.max_x(), 30.0);
    LASPP_ASSERT_EQ(zero_bounds.max_y(), 40.0);

    // Test with invalid bounds
    QuadtreeSpatialIndex index_invalid;
    index_invalid.set_bounds(100.0f, 100.0f, 100.0f, 100.0f);
    index_invalid.set_levels(2);
    // With 2 levels, level_offset[2] = 5, so index 5 is the first cell
    Bound2D invalid_bounds = index_invalid.get_cell_bounds(5);
    LASPP_ASSERT_EQ(invalid_bounds.min_x(), 100.0);
    LASPP_ASSERT_EQ(invalid_bounds.min_y(), 100.0);
    LASPP_ASSERT_EQ(invalid_bounds.max_x(), 100.0);
    LASPP_ASSERT_EQ(invalid_bounds.max_y(), 100.0);

    // Test with higher level cell indices
    QuadtreeSpatialIndex index_high;
    index_high.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index_high.set_levels(3);
    // With 3 levels, level_offset[3] = 21
    // For a cell in the top-right quadrant at all levels:
    // Level 0: 11 (top-right) = 3
    // Level 1: 11 (top-right) = 3
    // Level 2: 11 (top-right) = 3
    // Cell path = 3 << 4 | 3 << 2 | 3 = 0b111111 = 63
    // Cell index = 21 + 63 = 84
    Bound2D high_bounds = index_high.get_cell_bounds(84);
    LASPP_ASSERT_GT(high_bounds.min_x(), 50.0);
    LASPP_ASSERT_GT(high_bounds.min_y(), 50.0);
    LASPP_ASSERT_LE(high_bounds.max_x(), 100.0);
    LASPP_ASSERT_LE(high_bounds.max_y(), 100.0);
  }

  // Test round-trip: get_cell_index -> get_cell_bounds
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(3);

    // Test several points
    std::vector<std::pair<double, double>> test_points = {
        {10.0, 10.0}, {30.0, 20.0}, {50.0, 50.0}, {70.0, 80.0},
        {90.0, 90.0}, {25.0, 75.0}, {5.0, 95.0},  {95.0, 5.0},
    };

    for (const auto& [x, y] : test_points) {
      // Get cell index for this point
      int32_t cell_index = index.get_cell_index(x, y);

      // Get bounds for this cell
      Bound2D bounds = index.get_cell_bounds(cell_index);

      // Verify the point is within the bounds
      LASPP_ASSERT_GE(x, bounds.min_x());
      LASPP_ASSERT_LT(x, bounds.max_x());
      LASPP_ASSERT_GE(y, bounds.min_y());
      LASPP_ASSERT_LT(y, bounds.max_y());

      // Verify bounds are valid
      LASPP_ASSERT_LT(bounds.min_x(), bounds.max_x());
      LASPP_ASSERT_LT(bounds.min_y(), bounds.max_y());
    }
  }

  // Test constructor from header and points
  {
    // Create a simple header
    LASHeader header;
    header.transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});
    // Use const_cast to get non-const access to bounds for testing
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 10.0});

    // Create some points
    std::vector<LASPointFormat0> points;
    for (int i = 0; i < 100; ++i) {
      LASPointFormat0 point;
      point.x = i * 1000;  // 0.001 scale, so this is i * 1.0
      point.y = i * 1000;
      point.z = i * 100;
      point.intensity = 100;
      point.bit_byte = 0;
      point.classification_byte = 0;
      point.scan_angle_rank = 0;
      point.user_data = 0;
      point.point_source_id = 0;
      points.push_back(point);
    }

    // Build spatial index from points
    QuadtreeSpatialIndex index(header, points, 10.0);

    // Verify the index was built
    LASPP_ASSERT_GT(index.num_cells(), 0);
    const auto& quadtree = index.quadtree_header();
    LASPP_ASSERT_GT(quadtree.levels, 0u);
    LASPP_ASSERT_EQ(quadtree.min_x, 0.0f);
    LASPP_ASSERT_EQ(quadtree.min_y, 0.0f);
    LASPP_ASSERT_EQ(quadtree.max_x, 100.0f);
    LASPP_ASSERT_EQ(quadtree.max_y, 100.0f);

    // Verify all points are in cells
    const auto& cells = index.cells();
    size_t total_points_in_cells = 0;
    for (const auto& [cell_index, cell] : cells) {
      total_points_in_cells += cell.number_points;
    }
    LASPP_ASSERT_EQ(total_points_in_cells, 100u);
  }

  // Test constructor with different point formats
  {
    LASHeader header;
    header.transform() = Transform({0.01, 0.01, 0.01}, {100.0, 200.0, 0.0});
    // Use const_cast to get non-const access to bounds for testing
    const_cast<Bound3D&>(header.bounds()).update({100.0, 200.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({200.0, 300.0, 50.0});

    std::vector<LASPointFormat1> points;
    for (int i = 0; i < 50; ++i) {
      LASPointFormat1 point;
      point.x = static_cast<int32_t>((100.0 + i) * 100);  // 0.01 scale, offset 100
      point.y = static_cast<int32_t>((200.0 + i) * 100);  // 0.01 scale, offset 200
      point.z = i * 10;
      point.intensity = 50;
      point.bit_byte = 0;
      point.classification_byte = 0;
      point.scan_angle_rank = 0;
      point.user_data = 0;
      point.point_source_id = 0;
      point.gps_time = GPSTime(static_cast<double>(i)).gps_time;
      points.push_back(point);
    }

    QuadtreeSpatialIndex index(header, points, 5.0);
    LASPP_ASSERT_GT(index.num_cells(), 0);
    const auto& cells = index.cells();
    size_t total_points = 0;
    for (const auto& [cell_index, cell] : cells) {
      total_points += cell.number_points;
    }
    LASPP_ASSERT_EQ(total_points, 50u);
  }

  // Test constructor with empty points
  {
    LASHeader header;
    header.transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});
    // Use const_cast to get non-const access to bounds for testing
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 10.0});

    std::vector<LASPointFormat0> points;
    QuadtreeSpatialIndex index(header, points);
    LASPP_ASSERT_EQ(index.num_cells(), 0);
  }

  // Test constructor with default tile_size
  {
    LASHeader header;
    header.transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});
    // Use const_cast to get non-const access to bounds for testing
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 10.0});

    std::vector<LASPointFormat0> points;
    for (int i = 0; i < 10; ++i) {
      LASPointFormat0 point;
      point.x = i * 1000;
      point.y = i * 1000;
      point.z = 0;
      point.intensity = 0;
      point.bit_byte = 0;
      point.classification_byte = 0;
      point.scan_angle_rank = 0;
      point.user_data = 0;
      point.point_source_id = 0;
      points.push_back(point);
    }

    QuadtreeSpatialIndex index(header, points);  // Uses default tile_size
    LASPP_ASSERT_GT(index.num_cells(), 0);
  }

  // Test find_cell_index with adaptive quadtree (cells at different levels)
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(3);

    // Calculate correct cell indices for different levels
    // For point (30.0, 10.0) at level 2:
    //   Level 0: x < 50, y < 50 -> quadrant 0 (bottom-left)
    //   Level 1: x < 25, y < 25 -> quadrant 0 (bottom-left)
    //   Cell path = 0 << 2 | 0 = 0
    //   Cell index = level_offset[2] + 0 = 5 + 0 = 5
    int32_t expected_cell_30_10 = index.get_cell_index(30.0, 10.0);
    // But we want level 2, so let's use get_cell_index_at_level
    // Actually, let's just add the cells that would naturally exist and test

    // Add a cell at level 1: right-bottom quadrant
    // For point (75.0, 25.0) at level 1:
    //   Level 0: x >= 50, y < 50 -> quadrant 1 (right-bottom)
    //   Cell path = 1
    //   Cell index = level_offset[1] + 1 = 1 + 1 = 2
    int32_t cell_index_level1 = index.get_cell_index_at_level(75.0, 25.0, 1);
    std::vector<PointInterval> intervals1;
    intervals1.push_back({0, 99});
    index.add_cell(cell_index_level1, 100, intervals1);

    // Add a cell at level 2: for point (30.0, 10.0)
    int32_t cell_index_level2 = index.get_cell_index_at_level(30.0, 10.0, 2);
    std::vector<PointInterval> intervals2;
    intervals2.push_back({100, 199});
    index.add_cell(cell_index_level2, 100, intervals2);

    // Test that find_cell_index finds the most specific cell
    int32_t cell1 = index.find_cell_index(30.0, 10.0);
    LASPP_ASSERT_EQ(cell1, cell_index_level2);

    // Test that find_cell_index falls back to parent cell
    int32_t cell2 = index.find_cell_index(75.0, 25.0);
    LASPP_ASSERT_EQ(cell2, cell_index_level1);

    // Test point that would be at level 3 but cell doesn't exist - should find level 1
    int32_t cell3 = index.find_cell_index(60.0, 10.0);
    LASPP_ASSERT_EQ(cell3, cell_index_level1);
  }

  // Test get_cell_level_from_index
  // The function returns the level index (0-based), where:
  // - Index 0 -> level 0 (root)
  // - Indices 1-4 -> level 0 (first subdivision, level_offset[1] = 1)
  // - Indices 5-20 -> level 1 (second subdivision, level_offset[2] = 5)
  // - Indices 21-84 -> level 2 (third subdivision, level_offset[3] = 21)
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(3);

    // Test root level (index 0)
    uint32_t level0 = index.get_cell_level_from_index(0);
    LASPP_ASSERT_EQ(level0, 0u);

    // Test level 1 cells (indices 1-4, level_offset[1] = 1)
    uint32_t level1 = index.get_cell_level_from_index(1);
    LASPP_ASSERT_EQ(level1, 0u);
    uint32_t level2 = index.get_cell_level_from_index(2);
    LASPP_ASSERT_EQ(level2, 0u);
    uint32_t level3 = index.get_cell_level_from_index(3);
    LASPP_ASSERT_EQ(level3, 0u);
    uint32_t level4 = index.get_cell_level_from_index(4);
    LASPP_ASSERT_EQ(level4, 0u);

    // Test level 2 cells (indices 5-20, level_offset[2] = 5)
    uint32_t level5 = index.get_cell_level_from_index(5);
    LASPP_ASSERT_EQ(level5, 1u);
    uint32_t level10 = index.get_cell_level_from_index(10);
    LASPP_ASSERT_EQ(level10, 1u);
    uint32_t level20 = index.get_cell_level_from_index(20);
    LASPP_ASSERT_EQ(level20, 1u);

    // Test level 3 cells (indices 21-84, level_offset[3] = 21)
    uint32_t level21 = index.get_cell_level_from_index(21);
    LASPP_ASSERT_EQ(level21, 2u);
    uint32_t level50 = index.get_cell_level_from_index(50);
    LASPP_ASSERT_EQ(level50, 2u);
    uint32_t level84 = index.get_cell_level_from_index(84);
    LASPP_ASSERT_EQ(level84, 2u);
  }

  // Test get_cell_bounds with cells at different levels
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(2);

    // Calculate the correct cell indices for the quadrants
    // Level 1 quadrants (level_offset=1):
    // - cell_path=0 (00): left-bottom [0, 50) x [0, 50) -> cell_index = 1
    // - cell_path=1 (01): right-bottom [50, 100) x [0, 50) -> cell_index = 2
    // - cell_path=2 (10): left-top [0, 50) x [50, 100) -> cell_index = 3
    // - cell_path=3 (11): right-top [50, 100) x [50, 100) -> cell_index = 4

    // Test level 1 cell (index 1): should cover [0, 50) x [0, 50) - left-bottom
    Bound2D bounds1 = index.get_cell_bounds(1);
    LASPP_ASSERT_EQ(bounds1.min_x(), 0.0);
    LASPP_ASSERT_EQ(bounds1.min_y(), 0.0);
    LASPP_ASSERT_EQ(bounds1.max_x(), 50.0);
    LASPP_ASSERT_EQ(bounds1.max_y(), 50.0);

    // Test level 1 cell (index 2): should cover [50, 100) x [0, 50) - right-bottom
    Bound2D bounds2 = index.get_cell_bounds(2);
    LASPP_ASSERT_EQ(bounds2.min_x(), 50.0);
    LASPP_ASSERT_EQ(bounds2.min_y(), 0.0);
    LASPP_ASSERT_EQ(bounds2.max_x(), 100.0);
    LASPP_ASSERT_EQ(bounds2.max_y(), 50.0);

    // Test level 2 cell (index 6): should cover [25, 50) x [0, 25)
    // Level 2: level_offset=5, cell_path=1 (01) -> cell_index = 6
    Bound2D bounds6 = index.get_cell_bounds(6);
    LASPP_ASSERT_EQ(bounds6.min_x(), 25.0);
    LASPP_ASSERT_EQ(bounds6.min_y(), 0.0);
    LASPP_ASSERT_EQ(bounds6.max_x(), 50.0);
    LASPP_ASSERT_EQ(bounds6.max_y(), 25.0);
  }

  // Test find_cell_index with no cells (should return -1)
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(2);

    int32_t cell = index.find_cell_index(50.0, 50.0);
    LASPP_ASSERT_EQ(cell, -1);
  }

  // Test find_cell_index with root cell only
  {
    QuadtreeSpatialIndex index;
    index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    index.set_levels(2);

    std::vector<PointInterval> intervals;
    intervals.push_back({0, 99});
    index.add_cell(0, 100, intervals);

    int32_t cell = index.find_cell_index(50.0, 50.0);
    LASPP_ASSERT_EQ(cell, 0);
  }

  return 0;
}
