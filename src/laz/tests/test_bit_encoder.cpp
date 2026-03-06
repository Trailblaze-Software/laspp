/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
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
    multi_bits[0][i] = static_cast<bool>(i % 2 + i % 3 + i % 5 + i % 7 + i % 11 + i % 13);
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
