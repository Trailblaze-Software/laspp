/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <random>

#include "las_point.hpp"
#include "laz/rgb12_encoder.hpp"
#include "utilities/assert.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::RGB12Encoder encoder(laspp::RGB12Encoder(laspp::ColorData{1, 2, 3}));
      encoder.encode(ostream, laspp::ColorData{5, 6, 7});
      encoder.encode(ostream, laspp::ColorData{5, 6, 7});
      encoder.encode(ostream, laspp::ColorData{12345, 12345, 12345});
      encoder.encode(ostream, laspp::ColorData{1347, 64, 4563});
    }

    LASPP_ASSERT_EQ(encoded_stream.str().size(), 18);

    {
      laspp::InStream instream(encoded_stream);
      laspp::RGB12Encoder decoder(laspp::RGB12Encoder(laspp::ColorData{1, 2, 3}));
      LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{5, 6, 7}));
      LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{5, 6, 7}));
      LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{12345, 12345, 12345}));
      LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{1347, 64, 4563}));
    }
  }
  {
    std::stringstream encoded_stream;
    std::vector<laspp::ColorData> random_color_data;
    std::mt19937 gen(42);
    for (size_t i = 0; i < 1000; i++) {
      random_color_data.emplace_back(laspp::ColorData{static_cast<uint16_t>(gen()),
                                                      static_cast<uint16_t>(gen()),
                                                      static_cast<uint16_t>(gen())});
    }

    {
      laspp::OutStream ostream(encoded_stream);
      laspp::RGB12Encoder encoder(laspp::RGB12Encoder(laspp::ColorData{0, 0, 0}));
      for (const auto& color_data : random_color_data) {
        encoder.encode(ostream, color_data);
      }
    }

    LASPP_ASSERT_EQ(encoded_stream.str().size(), 6221);

    {
      laspp::InStream instream(encoded_stream);
      laspp::RGB12Encoder decoder(laspp::RGB12Encoder(laspp::ColorData{0, 0, 0}));
      for (const auto& color_data : random_color_data) {
        LASPP_ASSERT_EQ(decoder.decode(instream), color_data);
      }
    }
  }

  return 0;
}
