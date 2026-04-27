/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <vector>

#include "laz/raw_bytes_encoder.hpp"
#include "laz/stream.hpp"

using namespace laspp;

namespace {

std::vector<std::byte> make_bytes(std::initializer_list<uint8_t> values) {
  std::vector<std::byte> result;
  result.reserve(values.size());
  for (uint8_t v : values) {
    result.emplace_back(static_cast<std::byte>(v));
  }
  return result;
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Constructor stores the initial bytes as last_value().
  {
    auto initial = make_bytes({0x00, 0x01, 0x02, 0x03});
    RawBytesEncoder encoder(initial);
    LASPP_ASSERT_EQ(encoder.last_value(), initial);
  }

  // Roundtrip a sequence of fixed-width payloads (Short = 2 bytes).
  {
    std::vector<std::vector<std::byte>> values;
    values.emplace_back(make_bytes({0x00, 0x00}));
    values.emplace_back(make_bytes({0xFF, 0xFF}));
    values.emplace_back(make_bytes({0x12, 0x34}));
    values.emplace_back(make_bytes({0xAB, 0xCD}));
    values.emplace_back(make_bytes({0x00, 0xFF}));

    std::stringstream encoded_stream;
    {
      OutStream ostream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      LASPP_ASSERT_EQ(encoder.last_value(), values[0]);
      for (const auto& value : values) {
        encoder.encode(ostream, value);
        LASPP_ASSERT_EQ(encoder.last_value(), value);
      }
    }

    {
      InStream istream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      LASPP_ASSERT_EQ(encoder.last_value(), values[0]);
      for (const auto& value : values) {
        LASPP_ASSERT_EQ(encoder.decode(istream), value);
        LASPP_ASSERT_EQ(encoder.last_value(), value);
      }
    }
  }

  // Roundtrip Integer-sized (4 byte) and Double-sized (8 byte) payloads.
  for (size_t width : {size_t{4}, size_t{8}}) {
    std::vector<std::vector<std::byte>> values;
    {
      std::vector<std::byte> initial(width, std::byte{0});
      values.push_back(initial);
    }
    {
      std::vector<std::byte> all_ff(width, std::byte{0xFF});
      values.push_back(all_ff);
    }
    {
      std::vector<std::byte> ascending(width);
      for (size_t i = 0; i < width; i++) {
        ascending[i] = static_cast<std::byte>(i * 17 + 3);
      }
      values.push_back(ascending);
    }
    {
      std::vector<std::byte> descending(width);
      for (size_t i = 0; i < width; i++) {
        descending[i] = static_cast<std::byte>(255 - i * 11);
      }
      values.push_back(descending);
    }

    std::stringstream encoded_stream;
    {
      OutStream ostream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      for (const auto& value : values) {
        encoder.encode(ostream, value);
      }
    }

    {
      InStream istream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      for (const auto& value : values) {
        LASPP_ASSERT_EQ(encoder.decode(istream), value);
        LASPP_ASSERT_EQ(encoder.last_value(), value);
      }
    }
  }

  // Single-byte payloads: every possible byte value should roundtrip.
  {
    std::vector<std::vector<std::byte>> values;
    values.reserve(256);
    for (size_t i = 0; i < 256; i++) {
      values.emplace_back(std::vector<std::byte>{static_cast<std::byte>(i)});
    }

    std::stringstream encoded_stream;
    {
      OutStream ostream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      for (const auto& value : values) {
        encoder.encode(ostream, value);
      }
    }

    {
      InStream istream(encoded_stream);
      RawBytesEncoder encoder(values[0]);
      for (const auto& value : values) {
        LASPP_ASSERT_EQ(encoder.decode(istream), value);
      }
    }
  }

  // Pseudo-random payloads of varying width to stress the arithmetic stream.
  {
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    constexpr size_t WIDTH = 8;
    constexpr size_t COUNT = 1024;

    std::vector<std::vector<std::byte>> values;
    values.reserve(COUNT);
    for (size_t i = 0; i < COUNT; i++) {
      std::vector<std::byte> v(WIDTH);
      for (size_t j = 0; j < WIDTH; j++) {
        v[j] = static_cast<std::byte>(byte_dist(rng));
      }
      values.push_back(std::move(v));
    }

    std::stringstream encoded_stream;
    std::vector<std::byte> initial(WIDTH, std::byte{0});
    {
      OutStream ostream(encoded_stream);
      RawBytesEncoder encoder(initial);
      for (const auto& value : values) {
        encoder.encode(ostream, value);
      }
    }

    {
      InStream istream(encoded_stream);
      RawBytesEncoder encoder(initial);
      for (const auto& value : values) {
        LASPP_ASSERT_EQ(encoder.decode(istream), value);
        LASPP_ASSERT_EQ(encoder.last_value(), value);
      }
    }
  }

  return 0;
}
