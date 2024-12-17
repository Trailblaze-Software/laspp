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

#include "laz/integer_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);

      laspp::IntegerEncoder<32> int_encoder;
      int_encoder.encode_int(ostream, 12442);
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 14);
      int_encoder.encode_int(ostream, 1);
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 0);
      int_encoder.encode_int(ostream, std::numeric_limits<int32_t>::max());
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 31);
      int_encoder.encode_int(ostream, std::numeric_limits<int32_t>::min());
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 32);
      int_encoder.encode_int(ostream, 0);
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 0);
      int_encoder.encode_int(ostream, -1);
      LASPP_ASSERT_EQ(int_encoder.prev_k(), 1);

      laspp::raw_encode(ostream, 2445, 36);
    }

    {
      laspp::InStream instream(encoded_stream);
      {
        laspp::IntegerEncoder<32> int_encoder;
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), 12442);
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), 1);
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), std::numeric_limits<int32_t>::max());
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), std::numeric_limits<int32_t>::min());
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), 0);
        LASPP_ASSERT_EQ(int_encoder.decode_int(instream), -1);
      }
      LASPP_ASSERT_EQ(laspp::raw_decode(instream, 36), 2445u);
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
