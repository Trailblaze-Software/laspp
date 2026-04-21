/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <cstring>
#include <sstream>
#include <stdexcept>

#include "las_header.hpp"
#include "las_point.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

struct TestInterval {
  uint32_t start;
  uint32_t end;
};

// Write a minimal spatial index stream. The quadtree header uses valid bounds and the given
// levels. The interval section contains the provided cells; each cell carries the given
// number_points header field and intervals verbatim (no fixup), allowing injection of bad data.
static std::stringstream make_spatial_index_stream(
    uint32_t levels, float min_x, float max_x, float min_y, float max_y,
    // Each entry: {cell_index, number_points_field, intervals}
    const std::vector<std::tuple<int32_t, uint32_t, std::vector<TestInterval>>>& cells = {}) {
  std::stringstream ss;

  auto write_u32 = [&](uint32_t v) { ss.write(reinterpret_cast<const char*>(&v), 4); };
  auto write_i32 = [&](int32_t v) { ss.write(reinterpret_cast<const char*>(&v), 4); };
  auto write_f32 = [&](float v) { ss.write(reinterpret_cast<const char*>(&v), 4); };

  // LASX outer header
  ss.write("LASX", 4);
  write_u32(0);  // version

  // QuadtreeHeader
  ss.write("LASS", 4);
  write_u32(0);  // type
  ss.write("LASQ", 4);
  write_u32(0);  // version
  write_u32(levels);
  write_u32(0);  // level_index
  write_u32(0);  // implicit_levels
  write_f32(min_x);
  write_f32(max_x);
  write_f32(min_y);
  write_f32(max_y);

  // Interval section
  ss.write("LASV", 4);
  write_u32(0);  // version
  write_u32(static_cast<uint32_t>(cells.size()));
  for (const auto& [cell_index, number_points, intervals] : cells) {
    write_i32(cell_index);
    write_u32(static_cast<uint32_t>(intervals.size()));
    write_u32(number_points);  // written verbatim — may not match actual intervals
    for (const auto& iv : intervals) {
      write_u32(iv.start);
      write_u32(iv.end);
    }
  }

  return ss;
}

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

  // Test constructor from header and points
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    // Create points at different locations to generate cells.
    // With scale 0.001: x = (i%10)*10000 -> 0-90, y = (i/10)*10000 -> 0-90 (i < 100).
    std::vector<LASPointFormat0> points(100);
    for (size_t i = 0; i < points.size(); ++i) {
      points[i].x = static_cast<int32_t>((i % 10) * 10000);  // 0.001 scale -> 0-90
      points[i].y = static_cast<int32_t>((i / 10) * 10000);  // 0.001 scale -> 0-90
      points[i].z = 0;
    }

    QuadtreeSpatialIndex index(header, points);

    const auto& quadtree_header = index.quadtree_header();
    LASPP_ASSERT_EQ(quadtree_header.min_x, 0.0f);
    LASPP_ASSERT_EQ(quadtree_header.min_y, 0.0f);
    LASPP_ASSERT_EQ(quadtree_header.max_x, 100.0f);
    LASPP_ASSERT_EQ(quadtree_header.max_y, 100.0f);
    LASPP_ASSERT_GT(quadtree_header.levels, 0u);
    LASPP_ASSERT_GT(index.num_cells(), 0u);
  }

  // Test write and read round-trip
  {
    std::stringstream ss;
    {
      LASHeader header;
      const_cast<Bound3D&>(header.bounds()).update({1.0, 2.0, 0.0});
      const_cast<Bound3D&>(header.bounds()).update({3.0, 4.0, 0.0});

      std::vector<LASPointFormat0> points(150);
      for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = static_cast<int32_t>(1000 + (i % 10) * 100);  // 0.001 scale
        points[i].y = static_cast<int32_t>(2000 + (i / 10) * 100);
        points[i].z = 0;
      }

      // Use a smaller tile_size to generate multiple levels
      // With bounds [1,2] to [3,4], max_dim = 2.0
      // With tile_size = 0.1, ratio = 2.0 / 0.1 = 20, so levels = ceil(log2(20)) = 5
      QuadtreeSpatialIndex spatial_index(header, points, 0.1);

      // Write to stream
      spatial_index.write(ss);
    }

    // Read back
    QuadtreeSpatialIndex read_index(ss);

    // Verify header
    const auto& read_header = read_index.quadtree_header();
    LASPP_ASSERT(memcmp(read_header.spatial_signature, "LASS", 4) == 0);
    LASPP_ASSERT_EQ(read_header.type, 0u);
    LASPP_ASSERT(memcmp(read_header.quadtree_signature, "LASQ", 4) == 0);
    LASPP_ASSERT_EQ(read_header.version, 0u);
    LASPP_ASSERT_EQ(read_header.min_x, 1.0f);
    LASPP_ASSERT_EQ(read_header.min_y, 2.0f);
    LASPP_ASSERT_EQ(read_header.max_x, 3.0f);
    LASPP_ASSERT_EQ(read_header.max_y, 4.0f);
    // With tile_size=0.1 and max_dim=2.0, ratio=20, so levels = ceil(log2(20)) = 5
    LASPP_ASSERT_EQ(read_header.levels, 5u);
    LASPP_ASSERT_GT(read_index.num_cells(), 0u);
  }

  // Test get_cell_index
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    std::vector<LASPointFormat0> points;               // Empty points, just testing get_cell_index
    QuadtreeSpatialIndex index(header, points, 25.0);  // tile_size=25 gives 2 levels

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
  }

  // Test get_cell_bounds
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    std::vector<LASPointFormat0> points;
    // Use tile_size=25 to get 2 levels: ceil(log2(100/25)) = ceil(log2(4)) = 2
    QuadtreeSpatialIndex index(header, points, 25.0);

    // Test bounds for root cell (index 0)
    Bound2D root_bounds = index.get_cell_bounds(0);
    LASPP_ASSERT_EQ(root_bounds.min_x(), 0.0);
    LASPP_ASSERT_EQ(root_bounds.min_y(), 0.0);
    LASPP_ASSERT_EQ(root_bounds.max_x(), 100.0);
    LASPP_ASSERT_EQ(root_bounds.max_y(), 100.0);

    // Test bounds for level 1 cells
    // Cell index 1 (cell_path=0) -> bottom-left quadrant [0, 50) x [0, 50)
    int32_t cell1 = index.get_cell_index_at_level(25.0, 25.0, 1);
    Bound2D bounds1 = index.get_cell_bounds(cell1);
    LASPP_ASSERT_EQ(bounds1.min_x(), 0.0);
    LASPP_ASSERT_EQ(bounds1.min_y(), 0.0);
    LASPP_ASSERT_EQ(bounds1.max_x(), 50.0);
    LASPP_ASSERT_EQ(bounds1.max_y(), 50.0);

    // Cell index 2 (cell_path=1) -> right-bottom quadrant [50, 100) x [0, 50)
    int32_t cell2 = index.get_cell_index_at_level(75.0, 25.0, 1);
    Bound2D bounds2 = index.get_cell_bounds(cell2);
    LASPP_ASSERT_EQ(bounds2.min_x(), 50.0);
    LASPP_ASSERT_EQ(bounds2.min_y(), 0.0);
    LASPP_ASSERT_EQ(bounds2.max_x(), 100.0);
    LASPP_ASSERT_EQ(bounds2.max_y(), 50.0);
  }

  // Test find_cell_index with adaptive quadtree
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    // Create points that will generate specific cells
    std::vector<LASPointFormat0> points(200);
    // Points in bottom-left quadrant
    for (size_t i = 0; i < 100; ++i) {
      points[i].x = static_cast<int32_t>(i * 100);  // 0-99 * 0.001 = 0-0.099
      points[i].y = static_cast<int32_t>(i * 100);
      points[i].z = 0;
    }
    // Points in top-right quadrant
    for (size_t i = 100; i < 200; ++i) {
      points[i].x = static_cast<int32_t>(50000 + (i - 100) * 100);  // 50-59.9
      points[i].y = static_cast<int32_t>(50000 + (i - 100) * 100);
      points[i].z = 0;
    }

    QuadtreeSpatialIndex spatial_index(header, points);

    // find_cell_index should find the most specific cell containing the point
    int32_t cell1 = spatial_index.find_cell_index(25.0, 25.0);
    LASPP_ASSERT_GE(cell1, 0);
    Bound2D bounds1 = spatial_index.get_cell_bounds(cell1);
    LASPP_ASSERT(bounds1.contains(25.0, 25.0));

    int32_t cell2 = spatial_index.find_cell_index(75.0, 75.0);
    LASPP_ASSERT_GE(cell2, 0);
    Bound2D bounds2 = spatial_index.get_cell_bounds(cell2);
    LASPP_ASSERT(bounds2.contains(75.0, 75.0));
  }

  // Test get_cell_level_from_index
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    std::vector<LASPointFormat0> points;
    QuadtreeSpatialIndex index(header, points, 25.0);  // 2 levels

    // Root level (index 0) -> level 0
    uint32_t level0 = index.get_cell_level_from_index(0);
    LASPP_ASSERT_EQ(level0, 0u);

    // Level 1 cells (indices 1-4) -> level 1 (first subdivision)
    int32_t cell1 = index.get_cell_index_at_level(25.0, 25.0, 1);
    uint32_t level1 = index.get_cell_level_from_index(cell1);
    LASPP_ASSERT_EQ(level1, 1u);

    // Level 2 cells (indices 5-20) -> level 2 (second subdivision)
    int32_t cell2 = index.get_cell_index_at_level(12.5, 12.5, 2);
    uint32_t level2 = index.get_cell_level_from_index(cell2);
    LASPP_ASSERT_EQ(level2, 2u);
  }

  // Test get_cell_index_at_level
  {
    LASHeader header;
    const_cast<Bound3D&>(header.bounds()).update({0.0, 0.0, 0.0});
    const_cast<Bound3D&>(header.bounds()).update({100.0, 100.0, 0.0});

    std::vector<LASPointFormat0> points;
    QuadtreeSpatialIndex index(header, points, 25.0);  // 2 levels

    // Test getting cell index at specific level
    int32_t cell_level1 = index.get_cell_index_at_level(30.0, 30.0, 1);
    int32_t cell_level2 = index.get_cell_index_at_level(30.0, 30.0, 2);

    LASPP_ASSERT_GE(cell_level1, 1);
    LASPP_ASSERT_GE(cell_level2, 5);

    // Level 2 should be more specific (higher index due to level offset)
    LASPP_ASSERT_GT(cell_level2, cell_level1);
  }

  // Test that levels > 16 is rejected when loading from stream
  {
    bool caught = false;
    try {
      auto ss = make_spatial_index_stream(17u, 0.0f, 100.0f, 0.0f, 100.0f);
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("levels") != std::string::npos,
                   "Expected 'levels' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for levels > 16");
  }

  // Test that inverted X bounds are rejected when loading from stream
  {
    bool caught = false;
    try {
      auto ss = make_spatial_index_stream(4u, 100.0f, 0.0f, 0.0f, 100.0f);  // min_x > max_x
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("Inverted bounds") != std::string::npos,
                   "Expected 'Inverted bounds' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for inverted X bounds");
  }

  // Test that inverted Y bounds are rejected when loading from stream
  {
    bool caught = false;
    try {
      auto ss = make_spatial_index_stream(4u, 0.0f, 100.0f, 100.0f, 0.0f);  // min_y > max_y
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("Inverted bounds") != std::string::npos,
                   "Expected 'Inverted bounds' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for inverted Y bounds");
  }

  // Test that a well-formed cell with valid intervals loads correctly
  {
    // Cell 5 (level-2 leaf), intervals [0,4] and [10,14] -> 10 points total
    auto ss = make_spatial_index_stream(2u, 0.0f, 100.0f, 0.0f, 100.0f,
                                        {{5, 10u, {{0u, 4u}, {10u, 14u}}}});
    QuadtreeSpatialIndex ok_index(ss);
    LASPP_ASSERT_EQ(ok_index.num_cells(), 1u);
    const auto& cell = ok_index.cells().at(5);
    LASPP_ASSERT_EQ(cell.number_points, 10u);
    LASPP_ASSERT_EQ(cell.intervals.size(), 2u);
  }

  // Test that an inverted interval (start > end) is rejected
  {
    bool caught = false;
    try {
      // Interval [20, 10] is inverted; number_points field is set consistently to avoid
      // masking the start>end error with a point-count mismatch error first.
      auto ss = make_spatial_index_stream(2u, 0.0f, 100.0f, 0.0f, 100.0f, {{5, 0u, {{20u, 10u}}}});
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("start") != std::string::npos && msg.find("end") != std::string::npos,
                   "Expected 'start'/'end' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for inverted interval start > end");
  }

  // Test that a number_points mismatch is rejected
  {
    bool caught = false;
    try {
      // Intervals [0,4] give 5 points; number_points header says 99 -> mismatch
      auto ss = make_spatial_index_stream(2u, 0.0f, 100.0f, 0.0f, 100.0f, {{5, 99u, {{0u, 4u}}}});
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("mismatch") != std::string::npos ||
                       msg.find("number_points") != std::string::npos,
                   "Expected 'mismatch'/'number_points' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for number_points mismatch");
  }

  // Test that duplicate cell_index entries in the stream are rejected
  {
    bool caught = false;
    try {
      // Two cells both claiming cell_index 5; each has a valid, consistent interval.
      auto ss = make_spatial_index_stream(2u, 0.0f, 100.0f, 0.0f, 100.0f,
                                          {{5, 5u, {{0u, 4u}}}, {5, 3u, {{10u, 12u}}}});
      QuadtreeSpatialIndex bad_index(ss);
    } catch (const std::runtime_error& e) {
      caught = true;
      std::string msg = e.what();
      LASPP_ASSERT(msg.find("Duplicate") != std::string::npos,
                   "Expected 'Duplicate' in error message, got: ", msg);
    }
    LASPP_ASSERT(caught, "Expected std::runtime_error for duplicate cell_index");
  }

  return 0;
}
