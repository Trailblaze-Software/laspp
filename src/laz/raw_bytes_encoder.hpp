/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/assert.hpp"

namespace laspp {

// Encoder for LAZ item types with LAZItemVersion::NoCompression:
// Short (1), Integer (2), Long (3), Float (4), Double (5).
// These types store bytes verbatim through the arithmetic stream using raw encoding
// (no entropy compression). They are only used in non-layered (PointwiseChunked) files.
class RawBytesEncoder {
  std::vector<std::byte> m_last_value;

 public:
  using EncodedType = std::vector<std::byte>;

  explicit RawBytesEncoder(const std::vector<std::byte>& initial_bytes)
      : m_last_value(initial_bytes) {}

  const std::vector<std::byte>& last_value() const { return m_last_value; }

  const std::vector<std::byte>& decode(InStream& in_stream) {
    for (auto& b : m_last_value) {
      b = static_cast<std::byte>(raw_decode(in_stream, 8));
    }
    return m_last_value;
  }

  void encode(OutStream& out_stream, const std::vector<std::byte>& bytes) {
    LASPP_ASSERT_EQ(bytes.size(), m_last_value.size());
    for (const auto& b : bytes) {
      raw_encode(out_stream, static_cast<uint64_t>(static_cast<uint8_t>(b)), 8);
    }
    // Fixed-width payloads must not change length; keep the stream layout stable.
    std::copy(bytes.begin(), bytes.end(), m_last_value.begin());
  }
};

}  // namespace laspp
