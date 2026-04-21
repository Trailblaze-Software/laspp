/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <random>
#include <sstream>
#include <vector>

#include "laz/bytes14_encoder.hpp"
#include "laz/layered_stream.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::vector<std::vector<std::byte>> values;
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(1), std::byte(2)});
    values.emplace_back(std::vector<std::byte>{std::byte(7), std::byte(255), std::byte(5)});
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(254), std::byte(27)});
    values.emplace_back(std::vector<std::byte>{std::byte(0), std::byte(1), std::byte(2)});
    values.emplace_back(std::vector<std::byte>{std::byte(7), std::byte(0), std::byte(52)});

    std::string combined_data;

    {
      LayeredOutStreams<Bytes14Encoder::NUM_LAYERS> out_streams;
      Bytes14Encoder encoder(values[0], 0);
      for (size_t i = 1; i < values.size(); i++) {
        encoder.encode(out_streams, values[i], 0);
      }

      std::stringstream combined_stream = out_streams.combined_stream();
      combined_data = combined_stream.str();
    }

    std::span<const std::byte> size_span(reinterpret_cast<const std::byte*>(combined_data.data()),
                                         Bytes14Encoder::NUM_LAYERS * sizeof(uint32_t));
    std::span<const std::byte> data_span(
        reinterpret_cast<const std::byte*>(combined_data.data()) + size_span.size(),
        combined_data.size() - size_span.size());

    LayeredInStreams<Bytes14Encoder::NUM_LAYERS> in_streams(size_span, data_span);

    Bytes14Encoder decoder(values[0], 0);
    LASPP_ASSERT_EQ(decoder.last_value(), values[0]);
    for (size_t i = 1; i < values.size(); i++) {
      std::vector<std::byte> decoded = decoder.decode(in_streams, 0);
      LASPP_ASSERT_EQ(decoded, values[i]);
      LASPP_ASSERT_EQ(decoder.last_value(), values[i]);
    }
  }

  {
    std::mt19937_64 gen(42);
    std::vector<std::vector<std::byte>> random_values;
    random_values.reserve(100);

    // Generate random byte arrays of varying sizes
    for (size_t i = 0; i < 100; i++) {
      size_t num_bytes = (i % 10) + 1;  // Sizes from 1 to 10 bytes
      std::vector<std::byte> bytes(num_bytes);
      for (size_t j = 0; j < num_bytes; j++) {
        bytes[j] = static_cast<std::byte>(gen() % 256);
      }
      random_values.push_back(bytes);
    }

    // Use first value as initial value
    std::vector<std::byte> initial_value = random_values[0];

    std::string combined_data;

    {
      LayeredOutStreams<Bytes14Encoder::NUM_LAYERS> out_streams;
      Bytes14Encoder encoder(initial_value, 0);
      for (size_t i = 1; i < random_values.size(); i++) {
        // Pad or truncate to match initial size for this test
        std::vector<std::byte> value = random_values[i];
        if (value.size() != initial_value.size()) {
          value.resize(initial_value.size());
        }
        encoder.encode(out_streams, value, 0);
      }

      std::stringstream combined_stream = out_streams.combined_stream();
      combined_data = combined_stream.str();
    }

    std::span<const std::byte> size_span(reinterpret_cast<const std::byte*>(combined_data.data()),
                                         Bytes14Encoder::NUM_LAYERS * sizeof(uint32_t));
    std::span<const std::byte> data_span(
        reinterpret_cast<const std::byte*>(combined_data.data()) + size_span.size(),
        combined_data.size() - size_span.size());

    LayeredInStreams<Bytes14Encoder::NUM_LAYERS> in_streams(size_span, data_span);

    Bytes14Encoder decoder(initial_value, 0);
    for (size_t i = 1; i < random_values.size(); i++) {
      std::vector<std::byte> expected = random_values[i];
      if (expected.size() != initial_value.size()) {
        expected.resize(initial_value.size());
      }
      std::vector<std::byte> decoded = decoder.decode(in_streams, 0);
      LASPP_ASSERT_EQ(decoded, expected);
    }
  }

  {
    // Test context switching
    std::vector<std::byte> initial_value{std::byte(0), std::byte(1), std::byte(2)};
    std::vector<std::vector<std::byte>> values;
    values.emplace_back(std::vector<std::byte>{std::byte(10), std::byte(11), std::byte(12)});
    values.emplace_back(std::vector<std::byte>{std::byte(20), std::byte(21), std::byte(22)});
    values.emplace_back(std::vector<std::byte>{std::byte(30), std::byte(31), std::byte(32)});

    std::string combined_data;

    {
      LayeredOutStreams<Bytes14Encoder::NUM_LAYERS> out_streams;
      Bytes14Encoder encoder(initial_value, 0);
      encoder.encode(out_streams, values[0], 0);
      encoder.encode(out_streams, values[1], 1);  // Switch to context 1
      encoder.encode(out_streams, values[2], 2);  // Switch to context 2

      std::stringstream combined_stream = out_streams.combined_stream();
      combined_data = combined_stream.str();
    }

    std::span<const std::byte> size_span(reinterpret_cast<const std::byte*>(combined_data.data()),
                                         Bytes14Encoder::NUM_LAYERS * sizeof(uint32_t));
    std::span<const std::byte> data_span(
        reinterpret_cast<const std::byte*>(combined_data.data()) + size_span.size(),
        combined_data.size() - size_span.size());

    LayeredInStreams<Bytes14Encoder::NUM_LAYERS> in_streams(size_span, data_span);

    Bytes14Encoder decoder(initial_value, 0);
    std::vector<std::byte> decoded1 = decoder.decode(in_streams, 0);
    LASPP_ASSERT_EQ(decoded1, values[0]);

    std::vector<std::byte> decoded2 = decoder.decode(in_streams, 1);
    LASPP_ASSERT_EQ(decoded2, values[1]);

    std::vector<std::byte> decoded3 = decoder.decode(in_streams, 2);
    LASPP_ASSERT_EQ(decoded3, values[2]);
  }

  return 0;
}
