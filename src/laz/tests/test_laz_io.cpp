/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#include <sstream>

#include "las_point.hpp"
#include "laz/laz_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "laz/laz_writer.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  {
    std::stringstream stream;
    std::unique_ptr<LAZSpecialVLR> laz_special_vlr;

    {
      LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
      laz_special_vlr = std::make_unique<LAZSpecialVLR>(writer.special_vlr());
    }

    {
      LAZReader reader(*laz_special_vlr);
      reader.read_chunk_table(stream, 0);

      LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 0);
    }
  }

  //{
  // std::stringstream stream;
  // std::unique_ptr<LAZSpecialVLR> laz_special_vlr;
  //
  // std::vector<LASPointFormat1> points;
  // for (int i = 0; i < 10; i++) {
  // LASPointFormat1 point;
  // point.x = i;
  // point.y = i;
  // point.z = i;
  // points.push_back(point);
  //}
  //
  //{
  // LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
  // laz_special_vlr = std::make_unique<LAZSpecialVLR>(writer.special_vlr());
  //
  // std::stringstream compressed_chunk = writer.compress_chunk(std::span<LASPointFormat1>(points));
  //}
  //
  // std::cout << "compressed size: " << stream.str().size() << std::endl;
  //
  //{
  // LAZReader reader(*laz_special_vlr);
  // reader.read_chunk_table(stream, 10);
  // std::vector<LASPointFormat1> decompressed_points(34);
  //
  // size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
  // std::vector<std::byte> compressed_chunk(compressed_size);
  // stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
  // static_cast<int64_t>(compressed_size)); std::span<LASPointFormat1> points_span =
  // reader.decompress_chunk(compressed_chunk, std::span<LASPointFormat1>(decompressed_points));
  //
  // LASPP_ASSERT_EQ(points_span.size(), points.size());
  //
  //}
  //}
  return 0;
}
