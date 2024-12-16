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
#include <sstream>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/stream.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::BitSymbolEncoder bit_encoder;
      bit_encoder.encode_bit(ostream, 0);
      bit_encoder.encode_bit(ostream, 1);
      bit_encoder.encode_bit(ostream, 1);
      bit_encoder.encode_bit(ostream, 1);
      bit_encoder.encode_bit(ostream, 0);
      bit_encoder.encode_bit(ostream, 1);
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder bit_encoder;
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 0);
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 0);
      LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), 1);
    }
  }

  std::vector<std::vector<bool>> multi_bits;
  multi_bits.emplace_back(1 << 14);
  for (size_t i = 0; i < multi_bits[0].size(); i++) {
    multi_bits[0][i] = i % 2 + i % 3 + i % 5 + i % 7 + i % 11 + i % 13;
  }
  multi_bits.emplace_back(1 << 14);
  for (size_t i = 0; i < multi_bits[1].size(); i++) {
    multi_bits[1][i] = 0;
  }
  for (const auto& bits : multi_bits) {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::BitSymbolEncoder bit_encoder;
      for (bool bit : bits) {
        bit_encoder.encode_bit(ostream, bit);
      }
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder bit_encoder;
      for (bool bit : bits) {
        LASPP_ASSERT_EQ(bit_encoder.decode_bit(instream), bit);
      }
    }
  }

  return 0;
}
