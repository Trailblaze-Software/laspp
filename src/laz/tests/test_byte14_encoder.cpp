/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <memory>
#include <random>
#include <sstream>
#include <vector>

#include "laz/byte14_encoder.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

// Encode a sequence of per-slot byte values and return the encoded stream strings.
// values[0] is the initial (seed) value; values[1..] are encoded.
static std::string encode_slot(const std::vector<std::byte>& values, uint8_t initial_context = 0,
                               const std::vector<uint8_t>& contexts = {}) {
  LASPP_ASSERT_GT(values.size(), 0);
  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  LayeredOutStreams<1> out;

  Byte14Encoder encoder(values[0], initial_context);
  for (size_t i = 1; i < values.size(); i++) {
    uint8_t ctx = contexts.empty() ? initial_context : contexts[i - 1];
    encoder.encode(out, values[i], ctx);
  }
  out.layer_sizes();  // finalise
  return out.cb().str();
}

// Decode a stream of bytes using Byte14Encoder, verifying against expected values.
static void decode_and_check(const std::string& encoded, const std::vector<std::byte>& values,
                             uint8_t initial_context = 0,
                             const std::vector<uint8_t>& contexts = {}) {
  std::span<const std::byte> size_span(reinterpret_cast<const std::byte*>(encoded.data()),
                                       encoded.size() + sizeof(uint32_t));
  // LayeredInStreams<1> needs a layer-size header followed by the data.
  // Build a buffer: [uint32_t size][data].
  uint32_t data_size = static_cast<uint32_t>(encoded.size());
  std::vector<std::byte> buf(sizeof(uint32_t) + encoded.size());
  std::memcpy(buf.data(), &data_size, sizeof(uint32_t));
  std::memcpy(buf.data() + sizeof(uint32_t), encoded.data(), encoded.size());

  std::span<const std::byte> size_sp(buf.data(), sizeof(uint32_t));
  std::span<const std::byte> data_sp(buf.data() + sizeof(uint32_t), encoded.size());

  LayeredInStreams<1> in(size_sp, data_sp);
  Byte14Encoder decoder(values[0], initial_context);
  LASPP_ASSERT_EQ(decoder.last_value(), values[0]);
  for (size_t i = 1; i < values.size(); i++) {
    uint8_t ctx = contexts.empty() ? initial_context : contexts[i - 1];
    std::byte got = decoder.decode(in, ctx);
    LASPP_ASSERT_EQ(got, values[i]);
    LASPP_ASSERT_EQ(decoder.last_value(), values[i]);
  }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Simple sequence
  {
    std::vector<std::byte> vals = {std::byte(0), std::byte(7), std::byte(0), std::byte(0),
                                   std::byte(7)};
    auto enc = encode_slot(vals);
    decode_and_check(enc, vals);
  }

  // Random values
  {
    std::mt19937_64 gen(42);
    std::vector<std::byte> vals(100);
    for (auto& b : vals) b = static_cast<std::byte>(gen() % 256);

    auto enc = encode_slot(vals);
    decode_and_check(enc, vals);
  }

  // Context switching
  {
    std::vector<std::byte> vals = {std::byte(0), std::byte(10), std::byte(20), std::byte(30)};
    std::vector<uint8_t> contexts = {0, 1, 2};
    auto enc = encode_slot(vals, 0, contexts);
    decode_and_check(enc, vals, 0, contexts);
  }

  return 0;
}
