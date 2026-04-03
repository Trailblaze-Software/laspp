/*
 * SPDX-FileCopyrightText: (c) 2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

/*
 * SPDX-FileCopyrightText: (c) 2025 Trailblaze Software, all rights reserved
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

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "laz/layered_stream.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/assert.hpp"

namespace laspp {

class Bytes14Encoder {
  struct Context {
    bool initialized;
    std::vector<std::byte> last_value;
    std::vector<SymbolEncoder<256>> byte_encoders;  // One encoder per byte position

    Context() : initialized(false) {}

    void initialize(size_t num_bytes) {
      if (byte_encoders.empty()) {
        byte_encoders.resize(num_bytes);
      }
    }
  };

  std::array<Context, 4> m_contexts;
  uint8_t m_active_context;
  uint8_t m_last_value_context;
  size_t m_num_bytes;

  Context& ensure_context(uint8_t idx) {
    if (!m_contexts[idx].initialized) {
      LASPP_ASSERT(m_contexts[m_active_context].initialized,
                   "Cannot switch to uninitialized context when no initialized context exists.");
      m_contexts[idx].last_value = m_contexts[m_active_context].last_value;
      m_contexts[idx].initialize(m_num_bytes);
      m_contexts[idx].initialized = true;
    }
    m_active_context = idx;
    return m_contexts[idx];
  }

 public:
  static constexpr int NUM_LAYERS = 1;

  using EncodedType = std::vector<std::byte>;

  const EncodedType& last_value() const { return m_contexts[m_last_value_context].last_value; }

  explicit Bytes14Encoder(const std::vector<std::byte>& initial_bytes, uint8_t context)
      : m_active_context(context), m_num_bytes(initial_bytes.size()) {
    m_contexts[context].initialize(m_num_bytes);
    m_contexts[context].last_value = initial_bytes;
    m_contexts[context].initialized = true;
    m_last_value_context = context;
  }

 public:
  std::vector<std::byte> decode(LayeredInStreams<NUM_LAYERS>& in_streams, uint8_t context_idx) {
    InStream& in_stream = in_streams[0];
    // Yet another cursed bug in LASzip where the last item
    // is not switched to the new context
    m_last_value_context = m_contexts[context_idx].initialized ? m_active_context : context_idx;
    std::vector<std::byte>& last_value = m_contexts[m_last_value_context].last_value;
    Context& context = ensure_context(context_idx);

    for (size_t i = 0; i < m_num_bytes; i++) {
      uint8_t diff = static_cast<uint8_t>(context.byte_encoders[i].decode_symbol(in_stream));
      last_value[i] = static_cast<std::byte>(static_cast<uint8_t>(last_value[i]) + diff);
    }

    return last_value;
  }

  void encode(LayeredOutStreams<NUM_LAYERS>& out_streams, const std::vector<std::byte>& bytes,
              uint8_t context_idx) {
    OutStream& out_stream = out_streams[0];
    // Yet another cursed bug in LASzip where the last item
    // is not switched to the new context
    std::vector<std::byte>& last_value = m_contexts[context_idx].initialized
                                             ? m_contexts[m_active_context].last_value
                                             : m_contexts[context_idx].last_value;
    Context& context = ensure_context(context_idx);

    LASPP_ASSERT_EQ(bytes.size(), m_num_bytes);

    // Encode deltas for each byte
    for (size_t i = 0; i < m_num_bytes; i++) {
      uint8_t diff = static_cast<uint8_t>(bytes[i]) - static_cast<uint8_t>(last_value[i]);
      context.byte_encoders[i].encode_symbol(out_stream, diff);
    }

    last_value = bytes;
  }
};

}  // namespace laspp
