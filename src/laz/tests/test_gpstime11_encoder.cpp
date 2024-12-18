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

#include <iomanip>

#include "laz/gpstime11_encoder.hpp"
#include "utilities/assert.hpp"

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

    LASPP_ASSERT_EQ(encoded_stream.str().size(), 22);

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
    std::vector<size_t> compressed_sizes;

    values.emplace_back(2000);
    for (size_t i = 0; i < values.back().size(); i++) {
      values[0][i] = static_cast<double>(i);
    }
    compressed_sizes.push_back(10376);

    values.emplace_back(1000);
    for (size_t i = 0; i < values.back().size(); i++) {
      if (i % 50 < 5) {
        values[1][i] = (7888823421312. * static_cast<double>(i));
      } else {
        values[1][i] = static_cast<double>(i % 5);
      }
    }
    compressed_sizes.push_back(6895);

    values.emplace_back(1000);
    for (size_t i = 0; i < values.back().size(); i++) {
      if (i % 50 < 5) {
        values[2][i] = (0.2131233 + 0.5123 * static_cast<double>(i));
      } else {
        values[2][i] = 0;
      }
    }
    compressed_sizes.push_back(900);

    values.emplace_back(1000);
    for (size_t i = 0; i < values.back().size(); i++) {
      if (i % 50 < 5) {
        values[3][i] = 15666123123 * static_cast<double>(i) + 1e34;
      } else if (i % 50 < 10) {
        values[3][i] = 1e-65 - 1e-76 * static_cast<double>(i);
      } else if (i % 50 < 15) {
        values[3][i] = 1e13 + 0.5123 * static_cast<double>(i);
      } else {
        values[3][i] = 15666123123123123123. * static_cast<double>(i) + 1e34;
      }
    }
    compressed_sizes.push_back(816);

    values.emplace_back();
    {
      for (size_t i = 0; i < 10; i++) {
        values[4].push_back(1e-10 + static_cast<double>(i) * 1e-20);
      }
      for (size_t i = 0; i < 10; i++) {
        values[4].push_back(1e-10 + static_cast<double>(i) * 501 * 1e-20);
      }
      for (size_t i = 0; i < 10; i++) {
        values[4].push_back(1e-10 + static_cast<double>(i) * 1e-20);
      }
      for (size_t i = 0; i < 10; i++) {
        values[4].push_back(1e-10 - static_cast<double>(i) * 11 * 1e-20);
      }
    }
    compressed_sizes.push_back(112);

    for (size_t i = 0; i < values.size(); i++) {
      const auto& vec = values[i];
      std::stringstream encoded_stream;
      {
        laspp::OutStream ostream(encoded_stream);
        laspp::GPSTime11Encoder encoder(laspp::GPSTime(12));
        for (auto value : vec) {
          std::cout << std::setprecision(20) << "encoding: " << value << std::endl;
          encoder.encode(ostream, laspp::GPSTime(value));
        }
      }

      LASPP_ASSERT_EQ(encoded_stream.str().size(), compressed_sizes[i]);

      {
        laspp::InStream instream(encoded_stream);
        laspp::GPSTime11Encoder encoder(laspp::GPSTime(12));
        LASPP_ASSERT_EQ(encoder.last_value(), laspp::GPSTime(12));
        for (auto value : vec) {
          LASPP_ASSERT_EQ(encoder.decode(instream), value);
          std::cout << std::setprecision(20) << "decoded: " << value << std::endl;
          LASPP_ASSERT_EQ(encoder.last_value(), value);
        }
      }
    }
  }

  return 0;
}
