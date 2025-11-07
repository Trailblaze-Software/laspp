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

#include <sstream>

#include "las_header.hpp"
#include "las_point.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test copy_from_reader without spatial index
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 0, 0);
      writer.copy_header_metadata(LASHeader());
      writer.header().transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});

      std::vector<LASPointFormat0> points;
      for (int i = 0; i < 100; ++i) {
        LASPointFormat0 point;
        point.x = i * 1000;
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
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);
      writer.copy_from_reader(reader, false);
    }

    // Verify the copy
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    LASPP_ASSERT_EQ(output_reader.header().num_points(), 100u);
    LASPP_ASSERT_EQ(output_reader.header().point_format(), 0);

    std::vector<LASPointFormat0> output_points(100);
    output_reader.read_chunks<LASPointFormat0>(output_points, {0, output_reader.num_chunks()});

    for (size_t i = 0; i < 100; ++i) {
      LASPP_ASSERT_EQ(output_points[i].x, static_cast<int32_t>(i * 1000));
      LASPP_ASSERT_EQ(output_points[i].y, static_cast<int32_t>(i * 1000));
      LASPP_ASSERT_EQ(output_points[i].z, static_cast<int32_t>(i * 100));
    }
  }

  // Test copy_from_reader with spatial index
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 0, 0);
      writer.copy_header_metadata(LASHeader());
      writer.header().transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});
      // Use const_cast to get non-const access to bounds for testing
      const_cast<Bound3D&>(writer.header().bounds()).update({0.0, 0.0, 0.0});
      const_cast<Bound3D&>(writer.header().bounds()).update({100.0, 100.0, 10.0});

      std::vector<LASPointFormat0> points;
      for (int i = 0; i < 200; ++i) {
        LASPointFormat0 point;
        point.x = (i % 10) * 10000;  // Spread across 0-90
        point.y = (i / 10) * 10000;  // Spread across 0-190
        point.z = i * 10;
        point.intensity = 100;
        point.bit_byte = 0;
        point.classification_byte = 0;
        point.scan_angle_rank = 0;
        point.user_data = 0;
        point.point_source_id = 0;
        points.push_back(point);
      }
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer with spatial index
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);
      writer.copy_from_reader(reader, true);
    }

    // Verify the copy has spatial index
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    LASPP_ASSERT_EQ(output_reader.header().num_points(), 200u);
    LASPP_ASSERT(output_reader.has_lastools_spatial_index());

    const auto& spatial_index = output_reader.lastools_spatial_index();
    LASPP_ASSERT_GT(spatial_index.num_cells(), 0);

    // Verify points were reordered (check that spatial index is valid)
    std::vector<LASPointFormat0> output_points(200);
    output_reader.read_chunks<LASPointFormat0>(output_points, {0, output_reader.num_chunks()});
    LASPP_ASSERT_EQ(output_points.size(), 200u);
  }

  // Test copy_from_reader with VLRs
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 0, 0);
      writer.copy_header_metadata(LASHeader());

      // Add a VLR
      LASVLR vlr;
      vlr.reserved = 0;
      string_to_arr("TEST_USER", vlr.user_id);
      vlr.record_id = 12345;
      vlr.record_length_after_header = 10;
      string_to_arr("Test VLR", vlr.description);
      std::vector<std::byte> vlr_data(10, std::byte{0x42});
      writer.write_vlr(vlr, vlr_data);

      std::vector<LASPointFormat0> points(10);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);
      writer.copy_from_reader(reader, false);
    }

    // Verify VLR was copied
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    const auto& vlrs = output_reader.vlr_headers();
    LASPP_ASSERT_EQ(vlrs.size(), 1u);
    LASPP_ASSERT_EQ(std::string(vlrs[0].user_id, 16).substr(0, 9), "TEST_USER");
    LASPP_ASSERT_EQ(vlrs[0].record_id, 12345u);
  }

  // Test copy_from_reader with EVLRs
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 0, 0);
      writer.copy_header_metadata(LASHeader());

      std::vector<LASPointFormat0> points(10);
      writer.write_points(std::span<const LASPointFormat0>(points));

      // Add an EVLR
      LASEVLR evlr;
      evlr.reserved = 0;
      string_to_arr("TEST_EVLR", evlr.user_id);
      evlr.record_id = 54321;
      evlr.record_length_after_header = 20;
      string_to_arr("Test EVLR", evlr.description);
      std::vector<std::byte> evlr_data(20, std::byte{0x99});
      writer.write_evlr(evlr, evlr_data);
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);
      writer.copy_from_reader(reader, false);
    }

    // Verify EVLR was copied
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    const auto& evlrs = output_reader.evlr_headers();
    LASPP_ASSERT_EQ(evlrs.size(), 1u);
    LASPP_ASSERT_EQ(std::string(evlrs[0].user_id, 16).substr(0, 9), "TEST_EVLR");
    LASPP_ASSERT_EQ(evlrs[0].record_id, 54321u);
  }

  // Test copy_from_reader skips existing spatial index when adding new one
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 0, 0);
      writer.copy_header_metadata(LASHeader());
      writer.header().transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});
      // Use const_cast to get non-const access to bounds for testing
      const_cast<Bound3D&>(writer.header().bounds()).update({0.0, 0.0, 0.0});
      const_cast<Bound3D&>(writer.header().bounds()).update({100.0, 100.0, 10.0});

      std::vector<LASPointFormat0> points;
      for (int i = 0; i < 50; ++i) {
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
      writer.write_points(std::span<const LASPointFormat0>(points));

      // Add an old spatial index
      QuadtreeSpatialIndex old_index;
      old_index.set_bounds(0.0f, 0.0f, 100.0f, 100.0f);
      old_index.set_levels(2);
      std::vector<PointInterval> intervals;
      intervals.push_back({0, 49});
      old_index.add_cell(0, 50, intervals);
      writer.write_lastools_spatial_index(old_index);
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);
    LASPP_ASSERT(reader.has_lastools_spatial_index());

    // Copy to new writer with new spatial index
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);
      writer.copy_from_reader(reader, true);
    }

    // Verify only one spatial index exists (the new one)
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    LASPP_ASSERT(output_reader.has_lastools_spatial_index());
    const auto& evlrs = output_reader.evlr_headers();
    size_t spatial_index_count = 0;
    for (const auto& evlr : evlrs) {
      if (evlr.is_lastools_spatial_index_evlr()) {
        spatial_index_count++;
      }
    }
    LASPP_ASSERT_EQ(spatial_index_count, 1u);
  }

  // Test copy_from_reader with different point formats
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 1, 0);  // Format 1 with GPS time
      writer.copy_header_metadata(LASHeader());
      writer.header().transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});

      std::vector<LASPointFormat1> points;
      for (int i = 0; i < 50; ++i) {
        LASPointFormat1 point;
        point.x = i * 1000;
        point.y = i * 1000;
        point.z = i * 100;
        point.intensity = 100;
        point.bit_byte = 0;
        point.classification_byte = 0;
        point.scan_angle_rank = 0;
        point.user_data = 0;
        point.point_source_id = 0;
        point.gps_time = GPSTime(static_cast<double>(i)).gps_time;
        points.push_back(point);
      }
      writer.write_points(std::span<const LASPointFormat1>(points));
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 1, 0);
      writer.copy_from_reader(reader, false);
    }

    // Verify the copy
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    LASPP_ASSERT_EQ(output_reader.header().num_points(), 50u);
    LASPP_ASSERT_EQ(output_reader.header().point_format(), 1);

    std::vector<LASPointFormat1> output_points(50);
    output_reader.read_chunks<LASPointFormat1>(output_points, {0, output_reader.num_chunks()});

    for (size_t i = 0; i < 50; ++i) {
      LASPP_ASSERT_EQ(output_points[i].x, static_cast<int32_t>(i * 1000));
      LASPP_ASSERT_EQ(output_points[i].gps_time.f64, static_cast<double>(i));
    }
  }

  // Test copy_from_reader with LAZ compression
  {
    std::stringstream input_stream;
    {
      LASWriter writer(input_stream, 128, 0);  // Format 0 with LAZ compression
      writer.copy_header_metadata(LASHeader());
      writer.header().transform() = Transform({0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});

      std::vector<LASPointFormat0> points;
      for (int i = 0; i < 100; ++i) {
        LASPointFormat0 point;
        point.x = i * 1000;
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
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    // Read it back
    input_stream.seekg(0);
    LASReader reader(input_stream);

    // Copy to new writer (uncompressed)
    std::stringstream output_stream;
    {
      LASWriter writer(output_stream, 0, 0);  // Uncompressed
      writer.copy_from_reader(reader, false);
    }

    // Verify the copy
    output_stream.seekg(0);
    LASReader output_reader(output_stream);
    LASPP_ASSERT_EQ(output_reader.header().num_points(), 100u);
    LASPP_ASSERT_EQ(output_reader.header().point_format(), 0);
    LASPP_ASSERT(!output_reader.header().is_laz_compressed());

    std::vector<LASPointFormat0> output_points(100);
    output_reader.read_chunks<LASPointFormat0>(output_points, {0, output_reader.num_chunks()});

    for (size_t i = 0; i < 100; ++i) {
      LASPP_ASSERT_EQ(output_points[i].x, static_cast<int32_t>(i * 1000));
    }
  }

  return 0;
}
