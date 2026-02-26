/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
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

#include "laz/byte_encoder.hpp"
#include "laz/stream.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::ByteEncoder byte_encoder(std::byte(0));
      byte_encoder.encode(ostream, static_cast<std::byte>(14));
      byte_encoder.encode(ostream, static_cast<std::byte>(255));
      byte_encoder.encode(ostream, static_cast<std::byte>(255));
      byte_encoder.encode(ostream, static_cast<std::byte>(1));
      byte_encoder.encode(ostream, static_cast<std::byte>(2));
      byte_encoder.encode(ostream, static_cast<std::byte>(1));
      byte_encoder.encode(ostream, static_cast<std::byte>(220));
      byte_encoder.encode(ostream, static_cast<std::byte>(0));
      byte_encoder.encode(ostream, static_cast<std::byte>(2));
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::ByteEncoder byte_encoder(std::byte(0));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(14));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(255));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(255));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(1));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(2));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(1));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(220));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(0));
      LASPP_ASSERT_EQ(byte_encoder.decode(instream), std::byte(2));
    }
  }

  {
    std::vector<std::vector<std::byte>> values;
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(1), std::byte(2)});
    values.emplace_back(std::vector<std::byte>{std::byte(7), std::byte(255), std::byte(5)});
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(254), std::byte(27)});
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(1), std::byte(2)});
    values.emplace_back(std::vector<std::byte>{std::byte(7), std::byte(0), std::byte(52)});

    std::stringstream encoded_stream;
    {
      OutStream outstream(encoded_stream);
      BytesEncoder bytes_encoder(values[0]);
      for (const auto& value : values) {
        bytes_encoder.encode(outstream, value);
      }
    }

    {
      InStream instream(encoded_stream);
      BytesEncoder bytes_encoder(values[0]);
      LASPP_ASSERT_EQ(bytes_encoder.last_value(), values[0]);
      for (const auto& value : values) {
        LASPP_ASSERT_EQ(bytes_encoder.decode(instream), value);
        LASPP_ASSERT_EQ(bytes_encoder.last_value(), value);
      }
    }
  }

  return 0;
}
