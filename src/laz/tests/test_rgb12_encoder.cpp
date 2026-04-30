/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <random>

#include "las_point.hpp"
#include "laz/rgb12_encoder.hpp"
#include "utilities/assert.hpp"

namespace {

template <class Encoder>
void roundtrip_basic_sequence(size_t expected_size) {
  std::stringstream encoded_stream;
  {
    laspp::OutStream ostream(encoded_stream);
    Encoder encoder{laspp::ColorData{1, 2, 3}};
    encoder.encode(ostream, laspp::ColorData{5, 6, 7});
    encoder.encode(ostream, laspp::ColorData{5, 6, 7});
    encoder.encode(ostream, laspp::ColorData{12345, 12345, 12345});
    encoder.encode(ostream, laspp::ColorData{1347, 64, 4563});
  }

  LASPP_ASSERT_EQ(encoded_stream.str().size(), expected_size);

  {
    laspp::InStream instream(encoded_stream);
    Encoder decoder{laspp::ColorData{1, 2, 3}};
    LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{5, 6, 7}));
    LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{5, 6, 7}));
    LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{12345, 12345, 12345}));
    LASPP_ASSERT_EQ(decoder.decode(instream), (laspp::ColorData{1347, 64, 4563}));
  }
}

template <class Encoder>
void roundtrip_random(size_t expected_size) {
  std::stringstream encoded_stream;
  std::vector<laspp::ColorData> random_color_data;
  std::mt19937 gen(42);
  for (size_t i = 0; i < 1000; i++) {
    random_color_data.emplace_back(laspp::ColorData{
        static_cast<uint16_t>(gen()), static_cast<uint16_t>(gen()), static_cast<uint16_t>(gen())});
  }

  {
    laspp::OutStream ostream(encoded_stream);
    Encoder encoder{laspp::ColorData{0, 0, 0}};
    for (const auto& color_data : random_color_data) {
      encoder.encode(ostream, color_data);
    }
  }

  LASPP_ASSERT_EQ(encoded_stream.str().size(), expected_size);

  {
    laspp::InStream instream(encoded_stream);
    Encoder decoder{laspp::ColorData{0, 0, 0}};
    for (const auto& color_data : random_color_data) {
      LASPP_ASSERT_EQ(decoder.decode(instream), color_data);
    }
  }
}

template <class Encoder>
void roundtrip_grayscale_sequence() {
  // All colors are gray (R == G == B). v2 should exploit this with the gray
  // bit; v1 has no gray shortcut but must still round-trip cleanly.
  std::stringstream encoded_stream;
  std::vector<laspp::ColorData> grays;
  for (uint16_t v : {uint16_t{0}, uint16_t{17}, uint16_t{17}, uint16_t{255}, uint16_t{4096},
                     uint16_t{4096}, uint16_t{40000}, uint16_t{65535}}) {
    grays.emplace_back(laspp::ColorData{v, v, v});
  }

  {
    laspp::OutStream ostream(encoded_stream);
    Encoder encoder{laspp::ColorData{0, 0, 0}};
    for (const auto& c : grays) {
      encoder.encode(ostream, c);
    }
  }

  {
    laspp::InStream instream(encoded_stream);
    Encoder decoder{laspp::ColorData{0, 0, 0}};
    for (const auto& c : grays) {
      LASPP_ASSERT_EQ(decoder.decode(instream), c);
    }
  }
}

template <class Encoder>
void roundtrip_repeated_value() {
  // A long run of identical colors should encode the symbol-mask of "0" each
  // time (no diff bytes follow). Useful coverage for the early-exit paths in
  // both v1 and v2.
  std::stringstream encoded_stream;
  const laspp::ColorData repeated{0xABCD, 0x1234, 0x5678};

  {
    laspp::OutStream ostream(encoded_stream);
    Encoder encoder{repeated};
    for (size_t i = 0; i < 256; i++) {
      encoder.encode(ostream, repeated);
    }
  }

  {
    laspp::InStream instream(encoded_stream);
    Encoder decoder{repeated};
    for (size_t i = 0; i < 256; i++) {
      LASPP_ASSERT_EQ(decoder.decode(instream), repeated);
    }
  }
}

template <class Encoder>
void roundtrip_extreme_byte_changes() {
  // Force every per-byte change bit on/off across consecutive points, with
  // values that exercise full-byte wrap-around in the (cur - last) residual
  // computation.
  std::stringstream encoded_stream;
  const std::vector<laspp::ColorData> colors = {
      laspp::ColorData{0x0000, 0x0000, 0x0000}, laspp::ColorData{0xFFFF, 0xFFFF, 0xFFFF},
      laspp::ColorData{0x0000, 0x0000, 0x0000}, laspp::ColorData{0x00FF, 0xFF00, 0x0F0F},
      laspp::ColorData{0xFF00, 0x00FF, 0xF0F0}, laspp::ColorData{0x1234, 0x5678, 0x9ABC},
      laspp::ColorData{0x1234, 0x5678, 0x9ABC},  // repeat
      laspp::ColorData{0x1235, 0x5678, 0x9ABC},  // only low byte of red changes
      laspp::ColorData{0x1235, 0x5778, 0x9ABC},  // only high byte of green changes
  };

  {
    laspp::OutStream ostream(encoded_stream);
    Encoder encoder{laspp::ColorData{0, 0, 0}};
    for (const auto& c : colors) {
      encoder.encode(ostream, c);
    }
  }

  {
    laspp::InStream instream(encoded_stream);
    Encoder decoder{laspp::ColorData{0, 0, 0}};
    for (const auto& c : colors) {
      LASPP_ASSERT_EQ(decoder.decode(instream), c);
    }
  }
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // v2 (the default RGB12 encoder used by the library)
  roundtrip_basic_sequence<laspp::RGB12EncoderV2>(18);
  roundtrip_random<laspp::RGB12EncoderV2>(6221);
  roundtrip_grayscale_sequence<laspp::RGB12EncoderV2>();
  roundtrip_repeated_value<laspp::RGB12EncoderV2>();
  roundtrip_extreme_byte_changes<laspp::RGB12EncoderV2>();

  // v1 (used when reading/writing LAZ files compressed with the v1 RGB12
  // codec - e.g. older laszip outputs).
  roundtrip_basic_sequence<laspp::RGB12EncoderV1>(22);
  roundtrip_random<laspp::RGB12EncoderV1>(6172);
  roundtrip_grayscale_sequence<laspp::RGB12EncoderV1>();
  roundtrip_repeated_value<laspp::RGB12EncoderV1>();
  roundtrip_extreme_byte_changes<laspp::RGB12EncoderV1>();

  return 0;
}
