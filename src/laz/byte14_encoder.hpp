/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "laz/layered_stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/assert.hpp"

namespace laspp {

// LAS 1.4 / LAZ "Byte14" extra-bytes encoder for a *single* byte position.
//
// LASzip's v4 format allocates one independent arithmetic stream per extra-byte
// position.  Use one Byte14Encoder per slot, each wired to a LayeredInStreams<1>
// or LayeredOutStreams<1>.
//
// The four scanner-channel contexts mirror the LASzip "context-switch quirk":
// the delta is applied to the *previous* context's last value rather than the
// newly-switched-to context's value (see decode/encode comments).
class Byte14Encoder {
  struct Context {
    bool initialized = false;
    std::byte last_value{};
    SymbolEncoder<256> encoder;
  };
  std::array<Context, 4> m_contexts{};
  uint8_t m_active_context;
  uint8_t m_last_value_context;

  Context& ensure_context(uint8_t idx) {
    if (!m_contexts[idx].initialized) {
      LASPP_ASSERT(m_contexts[m_active_context].initialized,
                   "Cannot switch to uninitialized context when no initialized context exists.");
      m_contexts[idx].last_value = m_contexts[m_active_context].last_value;
      m_contexts[idx].initialized = true;
    }
    m_active_context = idx;
    return m_contexts[idx];
  }

 public:
  explicit Byte14Encoder(std::byte initial, uint8_t context)
      : m_active_context(context), m_last_value_context(context) {
    m_contexts[context].last_value = initial;
    m_contexts[context].initialized = true;
  }

  std::byte last_value() const { return m_contexts[m_last_value_context].last_value; }

  // Decode one byte from a single-layer arithmetic stream.
  // The "cursed bug": the diff is applied to the *previous* context's last value,
  // not the newly-switched-to context's value.
  std::byte decode(LayeredInStreams<1>& stream, uint8_t ctx) {
    m_last_value_context = m_contexts[ctx].initialized ? m_active_context : ctx;
    std::byte& lv = m_contexts[m_last_value_context].last_value;
    Context& c = ensure_context(ctx);
    if (stream.non_empty(0)) {
      uint8_t diff = static_cast<uint8_t>(c.encoder.decode_symbol(stream[0]));
      lv = static_cast<std::byte>(static_cast<uint8_t>(lv) + diff);
    }
    return lv;
  }

  void encode(LayeredOutStreams<1>& stream, std::byte value, uint8_t ctx) {
    // Mirror decode-side "context-switch quirk": when switching to an uninitialized context,
    // apply the diff to the newly created context's last value; otherwise apply it to the
    // previous active context's last value.
    m_last_value_context = m_contexts[ctx].initialized ? m_active_context : ctx;
    std::byte& lv = m_contexts[m_last_value_context].last_value;
    Context& c = ensure_context(ctx);
    uint8_t diff = static_cast<uint8_t>(value) - static_cast<uint8_t>(lv);
    c.encoder.encode_symbol(stream[0], diff);
    lv = value;
  }
};

}  // namespace laspp
