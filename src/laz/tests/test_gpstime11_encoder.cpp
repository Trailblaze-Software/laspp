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

#include "laz/gpstime11_encoder.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::GPSTime11Encoder encoder(laspp::GPSTime(0));
      encoder.encode(ostream, laspp::GPSTime(0.0));
      encoder.encode(ostream, laspp::GPSTime(1.0));
      encoder.encode(ostream, laspp::GPSTime(2.0));
      encoder.encode(ostream, laspp::GPSTime(1.0));
      encoder.encode(ostream, laspp::GPSTime(0.0));
      encoder.encode(ostream, laspp::GPSTime(2.0));
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::GPSTime11Encoder encoder(laspp::GPSTime(0));
      LASPP_ASSERT_EQ(encoder.decode(instream), 0.0);
      LASPP_ASSERT_EQ(encoder.decode(instream), 1.0);
      LASPP_ASSERT_EQ(encoder.decode(instream), 2.0);
      LASPP_ASSERT_EQ(encoder.decode(instream), 1.0);
      LASPP_ASSERT_EQ(encoder.decode(instream), 0.0);
      LASPP_ASSERT_EQ(encoder.decode(instream), 2.0);
    }
  }

  {
    std::vector<std::vector<double>> values;

    values.emplace_back();
    values[0].reserve(2000);
    for (int i = 0; i < 2000; i++) {
      values[0].push_back(i);
    }

    values.emplace_back();
    values[1].reserve(1000);
    for (int i = 0; i < 1000; i++) {
      if (i % 50 < 5) {
        values[1].push_back(7888823421312 * i);
      } else {
        values[1].push_back(i % 5);
      }
    }

    values.emplace_back();
    values[2].reserve(1000);
    for (int i = 0; i < 1000; i++) {
      if (i % 50 < 5) {
        values[2].push_back(0.2131233 + 0.5123 * i);
      } else {
        values[2].push_back(0);
      }
    }

    for (const std::vector<double>& vec : values) {
      std::stringstream encoded_stream;
      {
        laspp::OutStream ostream(encoded_stream);
        laspp::GPSTime11Encoder encoder(laspp::GPSTime(12));
        for (auto value : vec) {
          encoder.encode(ostream, laspp::GPSTime(value));
        }
      }

      {
        laspp::InStream instream(encoded_stream);
        laspp::GPSTime11Encoder encoder(laspp::GPSTime(12));
        LASPP_ASSERT_EQ(encoder.last_value(), laspp::GPSTime(12));
        for (auto value : vec) {
          LASPP_ASSERT_EQ(encoder.decode(instream), value);
          LASPP_ASSERT_EQ(encoder.last_value(), value);
        }
      }
    }
  }

  return 0;
}
