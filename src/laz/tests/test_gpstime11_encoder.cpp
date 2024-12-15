#include "laz/gpstime11_encoder.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::GPSTime11Encoder encoder(0);
      encoder.encode(ostream, 0.0);
      encoder.encode(ostream, 1.0);
      encoder.encode(ostream, 2.0);
      encoder.encode(ostream, 1.0);
      encoder.encode(ostream, 0.0);
      encoder.encode(ostream, 2.0);
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::GPSTime11Encoder encoder(0);
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
    for (int i = 0; i < 10000; i++) {
      values[0].push_back(i);
    }

    values.emplace_back();
    for (int i = 0; i < 1000; i++) {
      if (i % 50 < 5) {
        values[1].push_back(7888823421312 * i);
      } else {
        values[1].push_back(i % 5);
      }
    }

    values.emplace_back();
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
        laspp::GPSTime11Encoder encoder(0);
        for (auto value : vec) {
          encoder.encode(ostream, value);
        }
      }

      {
        laspp::InStream instream(encoded_stream);
        laspp::GPSTime11Encoder encoder(0);
        for (auto value : vec) {
          LASPP_ASSERT_EQ(encoder.decode(instream), value);
        }
      }
    }
  }

  return 0;
}
