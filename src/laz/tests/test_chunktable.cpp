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

#include "laz/chunktable.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    LAZChunkTable chunktable;

    for (uint32_t i = 0; i < 10; i++) {
      chunktable.add_chunk(1000, i * 4000u);
    }
    chunktable.add_chunk(300, 42);

    std::stringstream ss;
    chunktable.write(ss);

    LASPP_ASSERT_EQ(ss.str().size(), 34);

    LAZChunkTable read_chunktable(ss, 1000, 10300);
    LASPP_ASSERT_EQ(read_chunktable.num_chunks(), 11);

    for (uint32_t i = 0; i < 11; i++) {
      LASPP_ASSERT_EQ(read_chunktable.chunk_offset(i), 8 + (i * (i - 1) * 2000u));
      LASPP_ASSERT_EQ(read_chunktable.decompressed_chunk_offsets()[i], i * 1000u);
      LASPP_ASSERT_EQ(read_chunktable.compressed_chunk_size(i), i == 10 ? 42 : i * 4000u);
      LASPP_ASSERT_EQ(read_chunktable.points_per_chunk()[i], i == 10 ? 300u : 1000u);
    }
    LASPP_ASSERT_EQ(read_chunktable.constant_chunk_size(), 1000);

    std::cout << read_chunktable << std::endl;
  }

  {
    LAZChunkTable chunktable;

    for (uint32_t i = 0; i < 20; i++) {
      chunktable.add_chunk(30u * i, 40u + i);
    }

    std::stringstream ss;
    chunktable.write(ss);

    LASPP_ASSERT_EQ(ss.str().size(), 49);

    LAZChunkTable read_chunktable(ss);
    LASPP_ASSERT_EQ(read_chunktable.num_chunks(), 20);

    std::cout << read_chunktable << std::endl;

    for (uint32_t i = 0; i < 20; i++) {
      LASPP_ASSERT_EQ(read_chunktable.chunk_offset(i), 8 + 40u * i + (i * (i - 1)) / 2);
      LASPP_ASSERT_EQ(read_chunktable.decompressed_chunk_offsets()[i], 30u * i * (i - 1) / 2);
      LASPP_ASSERT_EQ(read_chunktable.compressed_chunk_size(i), 40u + i);
      LASPP_ASSERT_EQ(read_chunktable.points_per_chunk()[i], 30u * i);
    }
    LASPP_ASSERT_EQ(read_chunktable.constant_chunk_size(), std::optional<uint32_t>());
  }

  return 0;
}
