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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"

class LASPoint {
  std::array<int32_t, 3> position;
  double gps_time;

 public:
  void operator=(const laspp::LASPointFormat0& point) {
    position[0] = point.x;
    position[1] = point.y;
    position[2] = point.z;
  }

  void operator=(const laspp::GPSTime& point) { gps_time = point; }

  operator laspp::LASPointFormat0() const {
    laspp::LASPointFormat0 point;
    point.x = position[0];
    point.y = position[1];
    point.z = position[2];
    point.intensity = 0;
    point.bit_byte = 0;
    point.classification_byte = 0;
    point.scan_angle_rank = 0;
    point.user_data = 0;
    point.point_source_id = 0;
    return point;
  }

  operator laspp::GPSTime() const { return laspp::GPSTime(gps_time); }

  friend std::ostream& operator<<(std::ostream& os, const LASPoint& point) {
    os << "Position: (" << point.position[0] << ", " << point.position[1] << ", "
       << point.position[2] << ")";
    os << " GPS Time: " << point.gps_time;
    return os;
  }
};

template <typename PointType>
void read_and_write_points(laspp::LASReader& reader, laspp::LASWriter& writer) {
  std::vector<PointType> points(reader.num_points());
  reader.read_chunks<PointType>(points, {0, reader.num_chunks()});
  std::cout << points[0] << std::endl;
  std::cout << points[points.size() - 1] << std::endl;
  writer.write_points<PointType>(points);
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <in_file> <out_file>" << std::endl;
    return 1;
  }

  std::filesystem::path in_file(argv[1]);
  std::ifstream ifs(in_file, std::ios::binary);
  if (!ifs) {
    std::cerr << "Failed to open " << in_file << std::endl;
    return 1;
  }
  laspp::LASReader reader(ifs);
  std::cout << reader.header() << std::endl;

  auto vlrs = reader.vlr_headers();
  for (const auto& vlr : vlrs) {
    std::cout << vlr << std::endl;
  }

  auto evlrs = reader.evlr_headers();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  std::filesystem::path out_file(argv[2]);
  std::ofstream ofs(out_file, std::ios::binary);
  if (!ofs) {
    std::cerr << "Failed to open " << out_file << std::endl;
    return 1;
  }

  std::string out_extension = out_file.extension().string();
  bool laz_compress;
  if (out_extension == ".laz") {
    laz_compress = true;
  } else if (out_extension == ".las") {
    laz_compress = false;
  } else {
    throw std::runtime_error("LAS++ Error: Output file must have .las or .laz extension");
  }

  {
    uint8_t point_format = reader.header().point_format();
    if (laz_compress) {
      point_format |= 1 << 7;
    } else {
      point_format &= ~(1 << 7);
    }
    laspp::LASWriter writer(ofs, point_format);

    for (const auto& vlr : reader.vlr_headers()) {
      if (vlr.is_laz_vlr()) {
        continue;
      }
      writer.write_vlr(vlr, reader.read_vlr_data(vlr));
    }

    writer.header().transform() = reader.header().transform();

    {
      std::vector<LASPoint> points(reader.num_points());

      reader.read_chunks<LASPoint>(points, {0, reader.num_chunks()});
      std::cout << points[0] << std::endl;
      std::cout << points[1] << std::endl;
      std::cout << points[1000] << std::endl;
      std::cout << points[points.size() - 1] << std::endl;

      writer.write_points<LASPoint>(points);
      // SWITCH_OVER_POINT_TYPE(reader.header().point_format(), read_and_write_points, reader,
      // writer);
    }
  }

  return 0;
}
