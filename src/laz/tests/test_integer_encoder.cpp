/*
 * Copyright (C) 2024 Trailblaze Software
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
 * with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For closed source licensing or development requests, contact
 * trailblaze.software@gmail.com
 */

#include <cstring>
#include <sstream>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

using namespace laspp;

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      symbol_encoder.encode_symbol(ostream, 14);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 2);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 0);
      symbol_encoder.encode_symbol(ostream, 2);
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 14);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 2);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 0);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 2);
    }
  }

  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      symbol_encoder.encode_bit(ostream, 0);

      laspp::IntegerEncoder<32> int_encoder;
      int_encoder.encode_int(ostream, 12442);
      int_encoder.encode_int(ostream, 1);

      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 1);
      laspp::raw_encode(ostream, 2445, 36);
      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 0);
      symbol_encoder.encode_bit(ostream, 1);
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 0);
      {
        laspp::IntegerEncoder<32> int_encoder;
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), 12442);
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), 1);
      }
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(laspp::raw_decode(instream, 36), 2445u);
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 0);
      LASPP_ASSERT_EQ(symbol_encoder.decode_bit(instream), 1);
    }
  }

  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::IntegerEncoder<32> int_encoder;
      for (int32_t i = -1000; i < 1000; i++) {
        int_encoder.encode_int(ostream, i);
      }
    }
    {
      laspp::InStream instream(encoded_stream);
      laspp::IntegerEncoder<32> int_encoder;
      for (int32_t i = -1000; i < 1000; i++) {
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), i);
      }
    }
  }

  return 0;
}
