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
#include <iomanip>
#include <iostream>
#include <string>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

// Helper to convert int32 coordinates to double using scale/offset
static inline double int32_to_double(int32_t coord, double scale, double offset) {
  return coord * scale + offset;
}

// Check if a point index is within any interval of a cell
static bool point_in_intervals(uint32_t point_index, const CellIntervals& cell) {
  for (const auto& interval : cell.intervals) {
    if (point_index >= interval.start && point_index <= interval.end) {
      return true;
    }
  }
  return false;
}

template <typename PointType>
void validate_spatial_index_for_points(LASReader& reader, const QuadtreeSpatialIndex& index) {
  // Get scale and offset for coordinate conversion
  double scale_x = reader.header().transform().scale_factors().x();
  double offset_x = reader.header().transform().offsets().x();
  double scale_y = reader.header().transform().scale_factors().y();
  double offset_y = reader.header().transform().offsets().y();

  // Read all points
  std::vector<PointType> points(reader.num_points());
  reader.read_chunks<PointType>(points, {0, reader.num_chunks()});

  // Statistics
  size_t total_points = points.size();
  size_t points_in_correct_intervals = 0;
  size_t points_in_correct_bounds = 0;
  size_t points_not_in_intervals = 0;
  size_t points_with_missing_cells = 0;
  size_t total_interval_range = 0;  // Sum of all interval ranges across all cells

  // Get all cells from the spatial index
  const auto& cells = index.cells();

  // Calculate total interval range across all cells
  for (const auto& [cell_index, cell] : cells) {
    for (const auto& interval : cell.intervals) {
      total_interval_range += (interval.end - interval.start + 1);
    }
  }

  // Debug: collect sample of calculated vs stored cell indices
  std::map<int32_t, size_t> calculated_cell_indices;
  std::map<int32_t, size_t> stored_cell_indices;
  for (const auto& [cell_index, cell] : cells) {
    stored_cell_indices[cell_index] = cell.number_points;
  }

  // Validate each point
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& point = points[i];
    uint32_t point_index = static_cast<uint32_t>(i);

    // Convert coordinates to double
    double x = int32_to_double(point.x, scale_x, offset_x);
    double y = int32_to_double(point.y, scale_y, offset_y);

    // Get the cell index for this point (searching up the tree if needed)
    int32_t cell_index = index.find_cell_index(x, y);
    LASPP_ASSERT(index.get_cell_bounds(cell_index).contains(x, y),
                 "find_cell_index returned a cell that does not contain the point", x, " ", y, " ",
                 cell_index);
    if (cell_index < 0) {
      std::cerr << "ERROR: No cell found for point " << i << " at (" << x << ", " << y << ")"
                << std::endl;
      points_with_missing_cells++;
      continue;
    }
    calculated_cell_indices[cell_index]++;

    // find_cell_index already found the cell, so it must exist
    const auto& cell = cells.at(cell_index);

    // Check if point is in the intervals for this cell
    bool in_intervals = point_in_intervals(point_index, cell);
    if (in_intervals) {
      points_in_correct_intervals++;
    } else {
      points_not_in_intervals++;
    }

    // Check if point is in the bounds for this cell
    Bound2D cell_bounds = index.get_cell_bounds(cell_index);
    bool in_bounds = cell_bounds.contains(x, y);
    if (in_bounds) {
      points_in_correct_bounds++;
    }
  }

  // Report results
  std::cout << "=== Spatial Index Validation Results ===" << std::endl;
  std::cout << "Total points: " << total_points << std::endl;
  std::cout << "Unique calculated cell indices: " << calculated_cell_indices.size() << std::endl;
  std::cout << "Unique stored cell indices: " << stored_cell_indices.size() << std::endl;

  // Debug: show sample of mismatched cell indices
  if (points_with_missing_cells > 0) {
    std::cout << std::endl << "=== Debug: Sample of Calculated Cell Indices ===" << std::endl;
    size_t sample_count = 0;
    for (const auto& [cell_index, count] : calculated_cell_indices) {
      if (stored_cell_indices.find(cell_index) == stored_cell_indices.end()) {
        std::cout << "  Calculated cell index " << cell_index << " (used by " << count
                  << " points) - NOT in spatial index" << std::endl;
        sample_count++;
        if (sample_count >= 10) {
          std::cout << "  ... (showing first 10)" << std::endl;
          break;
        }
      }
    }
    std::cout << std::endl << "=== Debug: Sample of Stored Cell Indices ===" << std::endl;
    sample_count = 0;
    for (const auto& [cell_index, count] : stored_cell_indices) {
      if (calculated_cell_indices.find(cell_index) == calculated_cell_indices.end()) {
        std::cout << "  Stored cell index " << cell_index << " (has " << count
                  << " points) - NOT calculated by any point" << std::endl;
        sample_count++;
        if (sample_count >= 10) {
          std::cout << "  ... (showing first 10)" << std::endl;
          break;
        }
      }
    }
    std::cout << std::endl;
  }
  std::cout << "Points in correct intervals: " << points_in_correct_intervals << " (" << std::fixed
            << std::setprecision(2)
            << (100.0 * static_cast<double>(points_in_correct_intervals) /
                static_cast<double>(total_points))
            << "%)" << std::endl;
  std::cout << "Points in correct bounds: " << points_in_correct_bounds << " ("
            << (100.0 * static_cast<double>(points_in_correct_bounds) /
                static_cast<double>(total_points))
            << "%)" << std::endl;
  std::cout << "Points NOT in intervals: " << points_not_in_intervals << " ("
            << (100.0 * static_cast<double>(points_not_in_intervals) /
                static_cast<double>(total_points))
            << "%)" << std::endl;
  std::cout << "Points with missing cells: " << points_with_missing_cells << " ("
            << (100.0 * static_cast<double>(points_with_missing_cells) /
                static_cast<double>(total_points))
            << "%)" << std::endl;
  std::cout << std::endl;
  std::cout << "=== Efficiency Analysis ===" << std::endl;
  std::cout << "Total points: " << total_points << std::endl;
  std::cout << "Total interval range: " << total_interval_range << std::endl;

  if (total_interval_range > 0) {
    double efficiency =
        100.0 * (static_cast<double>(total_points) / static_cast<double>(total_interval_range));
    std::cout << "Efficiency: " << std::fixed << std::setprecision(2) << efficiency << "%"
              << std::endl;
    std::cout << "  (Total points / Sum of all interval ranges)" << std::endl;
    std::cout << "  (Higher is better - indicates more compact intervals)" << std::endl;
  } else {
    std::cout << "Efficiency: N/A (no intervals found)" << std::endl;
  }

  // Validation checks
  bool all_valid = true;
  if (points_not_in_intervals > 0) {
    std::cerr << "ERROR: " << points_not_in_intervals
              << " points are not in the intervals for their cell!" << std::endl;
    all_valid = false;
  }
  if (points_with_missing_cells > 0) {
    std::cerr << "ERROR: " << points_with_missing_cells
              << " points have cell indices that don't exist in the spatial index!" << std::endl;
    all_valid = false;
  }

  if (all_valid) {
    std::cout << std::endl << "✓ All points are in the correct intervals!" << std::endl;
  } else {
    std::cout << std::endl << "✗ Validation failed!" << std::endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <las_file>" << std::endl;
    std::cerr << "  Validates the spatial index in a LAS/LAZ file" << std::endl;
    return 1;
  }

  std::string filename = argv[1];
  std::filesystem::path file_path(filename);
  LASReader reader(file_path);

  // Check if spatial index exists
  if (!reader.has_lastools_spatial_index()) {
    std::cerr << "ERROR: No spatial index found in file!" << std::endl;
    std::cerr << "  (Checked both EVLRs and .lax file)" << std::endl;
    return 1;
  }

  const auto& index = reader.lastools_spatial_index();
  const auto& header = index.quadtree_header();
  std::cout << "Spatial index found with " << index.num_cells() << " cells" << std::endl;
  std::cout << "Quadtree levels: " << header.levels << std::endl;
  std::cout << "Level index: " << header.level_index << std::endl;
  std::cout << "Implicit levels: " << header.implicit_levels << std::endl;
  std::cout << "Bounds: [" << header.min_x << ", " << header.min_y << "] to [" << header.max_x
            << ", " << header.max_y << "]" << std::endl;
  std::cout << std::endl;

  // Validate based on point format
  LASPP_SWITCH_OVER_POINT_TYPE(reader.header().point_format(), validate_spatial_index_for_points,
                               reader, index);

  return 0;
}
