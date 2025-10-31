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

#include <cstddef>
#include <random>
#include <sstream>
#include <stdexcept>

#include "las_point.hpp"
#include "laz/laz_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "laz/laz_writer.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  {
    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, 0);

        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 0);
        LASPP_ASSERT_THROWS(reader.chunk_table().chunk_offset(0), std::out_of_range);
      }
    }

    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      std::mt19937 gen(0);
      gen.seed(42);
      std::vector<LASPointFormat1> points;
      points.reserve(100);
      for (size_t i = 0; i < 100; i++) {
        LASPointFormat1 point;
        point.x = static_cast<int32_t>(gen());
        point.y = static_cast<int32_t>(gen());
        point.z = static_cast<int32_t>(gen());
        point.intensity = static_cast<uint16_t>(gen());
        point.bit_byte = static_cast<uint8_t>(gen());
        point.classification_byte = static_cast<uint8_t>(gen());
        point.scan_angle_rank = static_cast<uint8_t>(gen());
        point.user_data = static_cast<uint8_t>(gen());
        point.point_source_id = static_cast<uint16_t>(gen());
        point.gps_time.f64 = static_cast<double>(gen());
        points.push_back(point);
      }

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point10));

        writer.write_chunk(std::span<LASPointFormat1>(points));
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      LASPP_ASSERT_EQ(stream.str().size(), 2136);

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, points.size());
        std::vector<LASPointFormat1> decompressed_points(
            reader.chunk_table().points_per_chunk()[0]);

        size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
        std::vector<std::byte> compressed_chunk(compressed_size);
        stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
        stream.read(reinterpret_cast<char *>(compressed_chunk.data()),
                    static_cast<int64_t>(compressed_size));
        std::span<LASPointFormat1> points_span = reader.decompress_chunk(
            compressed_chunk, std::span<LASPointFormat1>(decompressed_points));

        LASPP_ASSERT_EQ(points_span.size(), points.size());
        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(static_cast<LASPointFormat0 &>(points_span[i]), points[i]);
        }
      }
    }
  }
  {
    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, 0);

        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 0);
        LASPP_ASSERT_THROWS(reader.chunk_table().chunk_offset(0), std::out_of_range);
      }
    }

    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      std::mt19937 gen(0);
      gen.seed(42);
      std::vector<LASPointFormat3> points;
      points.reserve(200);
      for (size_t i = 0; i < 200; i++) {
        LASPointFormat3 point;
        point.x = static_cast<int32_t>(gen());
        point.y = static_cast<int32_t>(gen());
        point.z = static_cast<int32_t>(gen());
        point.intensity = static_cast<uint16_t>(gen());
        point.bit_byte = static_cast<uint8_t>(gen());
        point.classification_byte = static_cast<uint8_t>(gen());
        point.scan_angle_rank = static_cast<uint8_t>(gen());
        point.user_data = static_cast<uint8_t>(gen());
        point.point_source_id = static_cast<uint16_t>(gen());
        point.gps_time.f64 = static_cast<double>(gen());
        point.red = static_cast<uint16_t>(gen());
        point.green = static_cast<uint16_t>(gen());
        point.blue = static_cast<uint16_t>(gen());
        points.push_back(point);
      }

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::GPSTime11));
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGB12));
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point10));

        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(0, 10));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(10, 20));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(30, 120));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(150, 1));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(151, 49));
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      LASPP_ASSERT_EQ(stream.str().size(), 7083);

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, points.size());
        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 5);

        std::vector<LASPointFormat3> decompressed_points(200);

        for (size_t i = 0; i < reader.chunk_table().num_chunks(); i++) {
          size_t compressed_size = reader.chunk_table().compressed_chunk_size(i);
          std::vector<std::byte> compressed_chunk(compressed_size);
          stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(i)));
          stream.read(reinterpret_cast<char *>(compressed_chunk.data()),
                      static_cast<int64_t>(compressed_size));
          reader.decompress_chunk(compressed_chunk,
                                  std::span<LASPointFormat3>(decompressed_points)
                                      .subspan(reader.chunk_table().decompressed_chunk_offsets()[i],
                                               reader.chunk_table().points_per_chunk()[i]));
        }

        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
        }
      }
    }
  }
  {
    std::stringstream stream;
    std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

    std::mt19937 gen(0);
    gen.seed(2024);
    std::vector<LASPointFormat7> points;
    points.reserve(150);
    for (size_t i = 0; i < 150; i++) {
      LASPointFormat7 point;
      point.x = static_cast<int32_t>(gen());
      point.y = static_cast<int32_t>(gen());
      point.z = static_cast<int32_t>(gen());
      point.intensity = static_cast<uint16_t>(gen());
      point.return_number = static_cast<uint8_t>(((gen() % 15) + 1) & 0x0F);
      point.number_of_returns = static_cast<uint8_t>(((gen() % 15) + 1) & 0x0F);
      if (point.return_number > point.number_of_returns) {
        point.return_number = point.number_of_returns;
      }
      point.classification_flags = static_cast<uint8_t>(gen() & 0x0F);
      point.scanner_channel = static_cast<uint8_t>(gen() & 0x3);
      point.scan_direction_flag = static_cast<uint8_t>(gen() & 0x1);
      point.edge_of_flight_line = static_cast<uint8_t>((gen() >> 1) & 0x1);
      point.classification = static_cast<LASClassification>(gen() % 23);
      point.user_data = static_cast<uint8_t>(gen());
      point.scan_angle = static_cast<uint16_t>(gen());
      point.point_source_id = static_cast<uint16_t>(gen());
      point.gps_time = static_cast<double>(gen());
      point.red = static_cast<uint16_t>(gen());
      point.green = static_cast<uint16_t>(gen());
      point.blue = static_cast<uint16_t>(gen());
      points.push_back(point);
    }

    {
      LAZWriter writer(stream, LAZCompressor::LayeredChunked);
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point14));
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGB14));

      writer.write_chunk(std::span<LASPointFormat7>(points));
      laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
    }

    LASPP_ASSERT_GT(stream.str().size(), 0);

    {
      LAZReader reader(*laz_special_vlr);
      reader.read_chunk_table(stream, points.size());
      std::vector<LASPointFormat7> decompressed_points(points.size());

      size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
      std::vector<std::byte> compressed_chunk(compressed_size);
      stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
      stream.read(reinterpret_cast<char *>(compressed_chunk.data()),
                  static_cast<int64_t>(compressed_size));
      reader.decompress_chunk(compressed_chunk, std::span<LASPointFormat7>(decompressed_points));

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
      }
    }
  }
  return 0;
}
