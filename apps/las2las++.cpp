/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "las_reader.hpp"
#include "las_writer.hpp"
#include "utilities/assert.hpp"

int main(int argc, char* argv[]) {
  bool add_spatial_index_flag = false;
  int file_arg_start = 1;

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--add-spatial-index" || std::string(argv[i]) == "-s") {
      add_spatial_index_flag = true;
      file_arg_start++;
    }
  }

  if (argc - file_arg_start != 2) {
    std::cerr << "Usage: " << argv[0] << " [--add-spatial-index|-s] <in_file> <out_file>"
              << std::endl;
    std::cerr
        << "  --add-spatial-index, -s: Add spatial index to output file (points will be reordered)"
        << std::endl;
    return 1;
  }

  std::string in_file_str = argv[file_arg_start];
  std::string out_file_str = argv[file_arg_start + 1];

  std::filesystem::path in_file(in_file_str);
  laspp::LASReader reader(in_file);
  std::cout << reader.header() << std::endl;

  std::filesystem::path out_file(out_file_str);
  std::fstream ofs;
  ofs.open(out_file, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  LASPP_ASSERT(ofs.is_open(), "Failed to open ", out_file);

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
      point_format &= static_cast<uint8_t>(~(1u << 7));
    }
    laspp::LASWriter writer(ofs, point_format);

    // Copy everything from reader to writer
    writer.copy_from_reader(reader, add_spatial_index_flag);

    LASPP_ASSERT_EQ(reader.num_points(), writer.header().num_points(),
                    "Number of points in output file does not match input file");
    LASPP_ASSERT_EQ(reader.header().num_points_by_return(), writer.header().num_points_by_return(),
                    "Number of points by return in output file does not match input file");
    LASPP_ASSERT_LE(reader.header().bounds().min_x(), writer.header().bounds().min_x(),
                    "Output file min x is less than input file min x");
    LASPP_ASSERT_GE(reader.header().bounds().max_x(), writer.header().bounds().max_x(),
                    "Output file max x is greater than input file max x");
    LASPP_ASSERT_LE(reader.header().bounds().min_y(), writer.header().bounds().min_y(),
                    "Output file min y is less than input file min y");
    LASPP_ASSERT_GE(reader.header().bounds().max_y(), writer.header().bounds().max_y(),
                    "Output file max y is greater than input file max y");
    LASPP_ASSERT_LE(reader.header().bounds().min_z(), writer.header().bounds().min_z(),
                    "Output file min z is less than input file min z");
    LASPP_ASSERT_GE(reader.header().bounds().max_z(), writer.header().bounds().max_z(),
                    "Output file max z is greater than input file max z");
    LASPP_ASSERT_EQ(reader.header().transform().scale_factors(),
                    writer.header().transform().scale_factors(),
                    "Scale factors in output file do not match input file");
    LASPP_ASSERT_EQ(reader.header().transform().offsets(), writer.header().transform().offsets(),
                    "Offsets in output file do not match input file");

    std::cout << writer.header() << std::endl;
  }

  return 0;
}
