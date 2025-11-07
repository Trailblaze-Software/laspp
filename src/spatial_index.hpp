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

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "las_header.hpp"
#include "utilities/assert.hpp"
#include "utilities/macros.hpp"

namespace laspp {

/**
 * LAStools quadtree-based spatial index format.
 *
 * This implements the LAStools spatial index format which uses:
 * - A quadtree structure for spatial organization
 * - Interval lists per cell to identify point ranges
 *
 * Format:
 * - "LASX" signature (4 bytes)
 * - version (U32, 4 bytes, little endian, currently 0)
 * - Quadtree:
 *   - "LASS" signature (4 bytes)
 *   - type (U32, 4 bytes, little endian, value = 0 for quadtree)
 *   - "LASQ" signature (4 bytes)
 *   - version (U32, 4 bytes, little endian, currently 0)
 *   - levels (U32, 4 bytes, little endian)
 *   - level_index (U32, 4 bytes, little endian, default 0)
 *   - implicit_levels (U32, 4 bytes, little endian, default 0)
 *   - min_x (F32, 4 bytes, little endian)
 *   - max_x (F32, 4 bytes, little endian)
 *   - min_y (F32, 4 bytes, little endian)
 *   - max_y (F32, 4 bytes, little endian)
 * - Interval:
 *   - "LASV" signature (4 bytes)
 *   - version (U32, 4 bytes, little endian, currently 0)
 *   - number_cells (U32, 4 bytes, little endian)
 *   - For each cell:
 *     - cell_index (I32, 4 bytes, little endian)
 *     - number_intervals (U32, 4 bytes, little endian)
 *     - number_points (U32, 4 bytes, little endian)
 *     - For each interval:
 *       - start (U32, 4 bytes, little endian)
 *       - end (U32, 4 bytes, little endian)
 */

struct LASPP_PACKED QuadtreeHeader {
  char spatial_signature[4];   // "LASS"
  uint32_t type;               // 0 = LAS_SPATIAL_QUAD_TREE
  char quadtree_signature[4];  // "LASQ"
  uint32_t version;            // 0
  uint32_t levels;
  uint32_t level_index;      // default 0
  uint32_t implicit_levels;  // default 0
  float min_x;
  float max_x;
  float min_y;
  float max_y;

  friend std::ostream& operator<<(std::ostream& os, const QuadtreeHeader& header) {
    os << "Spatial signature: " << std::string(header.spatial_signature, 4) << std::endl;
    os << "Type: " << header.type << std::endl;
    os << "Quadtree signature: " << std::string(header.quadtree_signature, 4) << std::endl;
    os << "Version: " << header.version << std::endl;
    os << "Levels: " << header.levels << std::endl;
    os << "Level index: " << header.level_index << std::endl;
    os << "Implicit levels: " << header.implicit_levels << std::endl;
    os << "Min X: " << header.min_x << std::endl;
    os << "Max X: " << header.max_x << std::endl;
    os << "Min Y: " << header.min_y << std::endl;
    os << "Max Y: " << header.max_y << std::endl;
    return os;
  }
};

struct LASPP_PACKED IntervalCellData {
  int32_t cell_index;
  uint32_t number_intervals;
  uint32_t number_points;
  // Followed by number_intervals pairs of (start, end) U32 values
};

struct PointInterval {
  uint32_t start;
  uint32_t end;

  friend std::ostream& operator<<(std::ostream& os, const PointInterval& interval) {
    os << "[" << interval.start << ", " << interval.end << "]";
    return os;
  }
};

struct CellIntervals {
  int32_t cell_index;
  uint32_t number_points;
  std::vector<PointInterval> intervals;

  friend std::ostream& operator<<(std::ostream& os, const CellIntervals& cell) {
    os << "Cell index: " << cell.cell_index << std::endl;
    os << "Number of points: " << cell.number_points << std::endl;
    os << "Number of intervals: " << cell.intervals.size();
    if (cell.intervals.size() == 1) {
      os << " (" << cell.intervals[0] << ")";
    }
    os << std::endl;
    return os;
  }
};

class QuadtreeSpatialIndex {
  QuadtreeHeader m_quadtree_header;
  std::map<int32_t, CellIntervals> m_cells;

  // Calculate level offset for a given level
  // level_offset[0] = 0
  // level_offset[l+1] = level_offset[l] + ((1<<l)*(1<<l))
  static uint32_t calculate_level_offset(uint32_t level) {
    uint32_t offset = 0;
    for (uint32_t l = 0; l < level; ++l) {
      offset += (1u << l) * (1u << l);  // 4^l
    }
    return offset;
  }

  // Get cell index for a point at a specific level (not necessarily the deepest level)
  int32_t get_cell_index_at_level(double x, double y, uint32_t target_level) const {
    if (target_level == 0 || m_quadtree_header.levels == 0) return 0;
    if (target_level > m_quadtree_header.levels) {
      target_level = m_quadtree_header.levels;
    }

    double min_x = static_cast<double>(m_quadtree_header.min_x);
    double min_y = static_cast<double>(m_quadtree_header.min_y);
    double max_x = static_cast<double>(m_quadtree_header.max_x);
    double max_y = static_cast<double>(m_quadtree_header.max_y);

    double dx = max_x - min_x;
    double dy = max_y - min_y;
    if (dx <= 0 || dy <= 0) return 0;

    // Build the quadtree path (cell index within the target level)
    uint32_t cell_path = 0;
    double cell_size_x = dx;
    double cell_size_y = dy;

    // Build index from root (MSB) to target level (LSB)
    for (uint32_t level = 0; level < target_level; ++level) {
      cell_size_x /= 2.0;
      cell_size_y /= 2.0;

      // Determine quadrant at this level
      uint32_t quadrant = 0;
      if (x >= min_x + cell_size_x) {
        quadrant |= 1;  // Right half (x bit)
        min_x += cell_size_x;
      }
      if (y >= min_y + cell_size_y) {
        quadrant |= 2;  // Top half (y bit)
        min_y += cell_size_y;
      }

      // Place bits at the correct position: root at MSB, leaves at LSB
      uint32_t shift = 2 * (target_level - 1 - level);
      cell_path |= (quadrant << shift);
    }

    // Add level offset for the target level
    uint32_t level_offset = calculate_level_offset(target_level);
    return static_cast<int32_t>(level_offset + cell_path);
  }

 public:
  QuadtreeSpatialIndex() {
    // Initialize with default values
    std::memcpy(m_quadtree_header.spatial_signature, "LASS",
                sizeof(m_quadtree_header.spatial_signature));
    m_quadtree_header.type = 0;  // LAS_SPATIAL_QUAD_TREE
    std::memcpy(m_quadtree_header.quadtree_signature, "LASQ",
                sizeof(m_quadtree_header.quadtree_signature));
    m_quadtree_header.version = 0;
    m_quadtree_header.levels = 0;
    m_quadtree_header.level_index = 0;
    m_quadtree_header.implicit_levels = 0;
    m_quadtree_header.min_x = 0.0f;
    m_quadtree_header.max_x = 0.0f;
    m_quadtree_header.min_y = 0.0f;
    m_quadtree_header.max_y = 0.0f;
  }

  explicit QuadtreeSpatialIndex(std::istream& is) {
    // Read "LASX" signature
    char signature[4];
    LASPP_CHECK_READ(is.read(signature, 4));
    if (memcmp(signature, "LASX", 4) != 0) {
      LASPP_ASSERT(false, "Invalid spatial index signature");
    }

    // Read version
    uint32_t version;
    LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&version), 4));
    LASPP_ASSERT_EQ(version, 0u);

    // Read quadtree header
    LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&m_quadtree_header), sizeof(QuadtreeHeader)));
    LASPP_ASSERT(memcmp(m_quadtree_header.spatial_signature, "LASS", 4) == 0,
                 "Invalid quadtree spatial signature");
    LASPP_ASSERT_EQ(m_quadtree_header.type, 0u);
    LASPP_ASSERT(memcmp(m_quadtree_header.quadtree_signature, "LASQ", 4) == 0,
                 "Invalid quadtree signature");

    // Read interval header
    char interval_signature[4];
    LASPP_CHECK_READ(is.read(interval_signature, 4));
    LASPP_ASSERT(memcmp(interval_signature, "LASV", 4) == 0, "Invalid interval signature");

    uint32_t interval_version;
    LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&interval_version), 4));
    LASPP_ASSERT_EQ(interval_version, 0u);

    uint32_t number_cells;
    LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&number_cells), 4));

    // Read cells
    for (uint32_t i = 0; i < number_cells; ++i) {
      // Read packed struct field by field to avoid alignment issues
      int32_t cell_index;
      uint32_t number_intervals;
      uint32_t number_points;
      LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&cell_index), sizeof(int32_t)));
      LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&number_intervals), sizeof(uint32_t)));
      LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&number_points), sizeof(uint32_t)));

      CellIntervals cell;
      cell.cell_index = cell_index;
      cell.number_points = number_points;
      cell.intervals.reserve(number_intervals);

      // Read intervals
      for (uint32_t j = 0; j < number_intervals; ++j) {
        PointInterval interval;
        LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&interval.start), 4));
        LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&interval.end), 4));
        cell.intervals.push_back(interval);
      }

      // Use try_emplace to construct in place, avoiding unnecessary copy
      // try_emplace doesn't move from cell if key already exists
      m_cells.try_emplace(cell_index, std::move(cell));
    }
  }

  // Build spatial index from points
  template <typename PointType = LASPointFormat0>
  QuadtreeSpatialIndex(const LASHeader& header, const std::vector<PointType>& points = {},
                       double tile_size = 50.0) {
    // Initialize header signatures
    memcpy(m_quadtree_header.spatial_signature, "LASS", 4);
    m_quadtree_header.type = 0;  // LAS_SPATIAL_QUAD_TREE
    memcpy(m_quadtree_header.quadtree_signature, "LASQ", 4);
    m_quadtree_header.version = 0;
    m_quadtree_header.level_index = 0;
    m_quadtree_header.implicit_levels = 0;

    // Get bounds from header
    double min_x = header.bounds().min_x();
    double min_y = header.bounds().min_y();
    double max_x = header.bounds().max_x();
    double max_y = header.bounds().max_y();

    // Calculate appropriate quadtree levels based on tile_size
    double dx = max_x - min_x;
    double dy = max_y - min_y;
    double max_dim = std::max(dx, dy);

    uint32_t levels = 0;
    if (max_dim > 0 && tile_size > 0) {
      levels = static_cast<uint32_t>(std::ceil(std::log2(max_dim / tile_size)));
      levels = std::max(1u, std::min(levels, 20u));  // Limit to reasonable range
    } else {
      levels = 4;  // Default
    }

    m_quadtree_header.levels = levels;
    m_quadtree_header.min_x = static_cast<float>(min_x);
    m_quadtree_header.min_y = static_cast<float>(min_y);
    m_quadtree_header.max_x = static_cast<float>(max_x);
    m_quadtree_header.max_y = static_cast<float>(max_y);

    // Get scale and offset for coordinate conversion
    double scale_x = header.transform().scale_factors().x();
    double offset_x = header.transform().offsets().x();
    double scale_y = header.transform().scale_factors().y();
    double offset_y = header.transform().offsets().y();

    // Helper to convert int32 coordinates to double using scale/offset
    auto int32_to_double = [](int32_t coord, double scale, double offset) {
      return coord * scale + offset;
    };

    // Group points by quadtree cell
    std::map<int32_t, std::vector<uint32_t>> cell_to_points;
    for (size_t i = 0; i < points.size(); ++i) {
      double x = int32_to_double(points[i].x, scale_x, offset_x);
      double y = int32_to_double(points[i].y, scale_y, offset_y);

      int32_t cell_index = get_cell_index(x, y);
      cell_to_points[cell_index].push_back(static_cast<uint32_t>(i));
    }

    // Build intervals for each cell
    // Intervals are consecutive point ranges within each cell
    for (const auto& [cell_index, point_indices] : cell_to_points) {
      std::vector<PointInterval> intervals;
      uint32_t interval_start = static_cast<uint32_t>(point_indices[0]);
      uint32_t interval_end = interval_start;

      for (size_t i = 1; i < point_indices.size(); ++i) {
        if (point_indices[i] == interval_end + 1) {
          // Consecutive point, extend interval
          interval_end = static_cast<uint32_t>(point_indices[i]);
        } else {
          // Gap found, save current interval and start new one
          intervals.push_back({interval_start, interval_end});
          interval_start = static_cast<uint32_t>(point_indices[i]);
          interval_end = interval_start;
        }
      }
      // Add final interval
      intervals.push_back({interval_start, interval_end});

      add_cell(cell_index, static_cast<uint32_t>(point_indices.size()), intervals);
    }
  }

  void set_bounds(float min_x, float min_y, float max_x, float max_y) {
    m_quadtree_header.min_x = min_x;
    m_quadtree_header.min_y = min_y;
    m_quadtree_header.max_x = max_x;
    m_quadtree_header.max_y = max_y;
  }

  void set_levels(uint32_t levels) { m_quadtree_header.levels = levels; }

  void add_cell(int32_t cell_index, uint32_t number_points,
                const std::vector<PointInterval>& intervals) {
    CellIntervals cell;
    cell.cell_index = cell_index;
    cell.number_points = number_points;
    cell.intervals = intervals;
    // Use try_emplace to construct in place, avoiding unnecessary copy
    // try_emplace doesn't move from cell if key already exists
    m_cells.try_emplace(cell_index, std::move(cell));
  }

  void write(std::ostream& os) const {
    // Write "LASX" signature
    os.write("LASX", 4);

    // Write version
    uint32_t version = 0;
    os.write(reinterpret_cast<const char*>(&version), 4);

    // Write quadtree header
    os.write(reinterpret_cast<const char*>(&m_quadtree_header), sizeof(QuadtreeHeader));

    // Write interval header
    os.write("LASV", 4);
    uint32_t interval_version = 0;
    os.write(reinterpret_cast<const char*>(&interval_version), 4);

    uint32_t number_cells = static_cast<uint32_t>(m_cells.size());
    os.write(reinterpret_cast<const char*>(&number_cells), 4);

    // Write cells
    for (const auto& [cell_index, cell] : m_cells) {
      IntervalCellData cell_data;
      cell_data.cell_index = cell_index;
      cell_data.number_intervals = static_cast<uint32_t>(cell.intervals.size());
      cell_data.number_points = cell.number_points;
      os.write(reinterpret_cast<const char*>(&cell_data), sizeof(IntervalCellData));

      // Write intervals
      for (const auto& interval : cell.intervals) {
        os.write(reinterpret_cast<const char*>(&interval.start), 4);
        os.write(reinterpret_cast<const char*>(&interval.end), 4);
      }
    }
  }

  const QuadtreeHeader& quadtree_header() const { return m_quadtree_header; }
  const std::map<int32_t, CellIntervals>& cells() const { return m_cells; }

  size_t num_cells() const { return m_cells.size(); }

  // Find the cell index for a point, searching up the quadtree if the exact cell doesn't exist
  // Returns the cell index of the nearest ancestor that exists in the spatial index
  // Returns -1 if no cell is found (shouldn't happen for valid spatial indexes)
  // This handles adaptive quadtrees where cells may be merged at higher levels
  int32_t find_cell_index(double x, double y) const {
    // Start from the deepest level and work up
    for (uint32_t level = m_quadtree_header.levels; level > 0; --level) {
      int32_t cell_index = get_cell_index_at_level(x, y, level);
      if (m_cells.find(cell_index) != m_cells.end()) {
        return cell_index;
      }
    }
    // Check root level
    if (m_cells.find(0) != m_cells.end()) {
      return 0;
    }
    return -1;  // No cell found (shouldn't happen)
  }

  // Get cell index for a given x, y coordinate
  // Returns the LAStools cell index which includes level offset
  // Quadtree encoding: root level at MSB, leaf level at LSB
  // Each level adds 2 bits: x bit (bit 1 of level), y bit (bit 0 of level)
  int32_t get_cell_index(double x, double y) const {
    if (m_quadtree_header.levels == 0) return 0;

    double min_x = static_cast<double>(m_quadtree_header.min_x);
    double min_y = static_cast<double>(m_quadtree_header.min_y);
    double max_x = static_cast<double>(m_quadtree_header.max_x);
    double max_y = static_cast<double>(m_quadtree_header.max_y);

    double dx = max_x - min_x;
    double dy = max_y - min_y;
    if (dx <= 0 || dy <= 0) return 0;

    // Build the quadtree path (cell index within the deepest level)
    uint32_t cell_path = 0;
    double cell_size_x = dx;
    double cell_size_y = dy;

    // Build index from root (MSB) to leaf (LSB)
    for (uint32_t level = 0; level < m_quadtree_header.levels; ++level) {
      cell_size_x /= 2.0;
      cell_size_y /= 2.0;

      // Determine quadrant at this level
      uint32_t quadrant = 0;
      if (x >= min_x + cell_size_x) {
        quadrant |= 1;  // Right half (x bit)
        min_x += cell_size_x;
      }
      if (y >= min_y + cell_size_y) {
        quadrant |= 2;  // Top half (y bit)
        min_y += cell_size_y;
      }

      // Place bits at the correct position: root at MSB, leaves at LSB
      // For level 0 (root): shift by 2*(levels-1) to put at MSB
      // For level n-1 (leaf): shift by 0 to put at LSB
      uint32_t shift = 2 * (m_quadtree_header.levels - 1 - level);
      cell_path |= (quadrant << shift);
    }

    // Add level offset for the deepest level (leaf level)
    // All cells are at the deepest level in our implementation
    uint32_t level_offset = calculate_level_offset(m_quadtree_header.levels);
    return static_cast<int32_t>(level_offset + cell_path);
  }

  // Determine which level a cell index belongs to
  uint32_t get_cell_level_from_index(int32_t cell_index) const {
    if (m_quadtree_header.levels == 0 || cell_index == 0) return 0;

    // Find the highest level offset that is <= cell_index
    for (uint32_t level = m_quadtree_header.levels; level > 0; --level) {
      uint32_t offset = calculate_level_offset(level);
      if (static_cast<uint32_t>(cell_index) >= offset) {
        return level - 1;  // Cell is at this level
      }
    }
    return 0;  // Root level
  }

  // Get cell bounds from cell index (reverse of get_cell_index)
  // The cell_index includes level offset, so we need to determine which level it's at
  // This handles adaptive quadtrees where cells can be at any level
  Bound2D get_cell_bounds(int32_t cell_index) const {
    if (m_quadtree_header.levels == 0) {
      return Bound2D(static_cast<double>(m_quadtree_header.min_x),
                     static_cast<double>(m_quadtree_header.min_y),
                     static_cast<double>(m_quadtree_header.max_x),
                     static_cast<double>(m_quadtree_header.max_y));
    }

    // Determine which level this cell belongs to
    uint32_t cell_level = get_cell_level_from_index(cell_index);

    // Subtract level offset for this cell's level to get the cell path
    uint32_t level_offset = calculate_level_offset(cell_level + 1);
    uint32_t cell_path = static_cast<uint32_t>(cell_index) - level_offset;

    double min_x = static_cast<double>(m_quadtree_header.min_x);
    double min_y = static_cast<double>(m_quadtree_header.min_y);
    double max_x = static_cast<double>(m_quadtree_header.max_x);
    double max_y = static_cast<double>(m_quadtree_header.max_y);

    double dx = max_x - min_x;
    double dy = max_y - min_y;
    if (dx <= 0 || dy <= 0) {
      return Bound2D(min_x, min_y, max_x, max_y);
    }

    double cell_size_x = dx;
    double cell_size_y = dy;
    double current_min_x = min_x;
    double current_min_y = min_y;

    // Extract bits from cell_path to determine cell position
    // The path is built with root at MSB, so we extract from MSB to LSB
    // Only process levels up to the cell's level
    for (uint32_t level = 0; level <= cell_level; ++level) {
      // Calculate shift to get the bits for this level (from MSB)
      uint32_t shift = 2 * (cell_level - level);
      uint32_t bits = (cell_path >> shift) & 3;

      cell_size_x /= 2.0;
      cell_size_y /= 2.0;

      if (bits & 1) {  // X bit
        current_min_x += cell_size_x;
      }
      if (bits & 2) {  // Y bit
        current_min_y += cell_size_y;
      }
    }

    return Bound2D(current_min_x, current_min_y, current_min_x + cell_size_x,
                   current_min_y + cell_size_y);
  }

  friend std::ostream& operator<<(std::ostream& os, const QuadtreeSpatialIndex& index) {
    os << "Quadtree Header:" << std::endl;
    os << index.m_quadtree_header << std::endl;
    os << "Cells:" << index.m_cells << std::endl;
    return os;
  }
};

}  // namespace laspp
