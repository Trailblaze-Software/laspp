/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>

#include "las_point.hpp"
#include "laz/layered_stream.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/arithmetic.hpp"

namespace laspp {

class RGBNIR14Encoder {
  struct Context {
    bool initialized = false;
    RGBNIRData last{};
    SymbolEncoder<1 << 7> rgb_bytes_used;
    std::array<SymbolEncoder<256>, 6> rgb_diff;
    SymbolEncoder<1 << 2> nir_bytes_used;
    std::array<SymbolEncoder<256>, 2> nir_diff;

    Context() = default;
  };

  std::array<Context, 4> m_contexts{};
  uint8_t m_active_context;
  bool m_use_laszip_v3_context_quirk = false;

  Context& ensure_context(uint8_t idx) {
    if (!m_contexts[idx].initialized) {
      LASPP_ASSERT(m_contexts[m_active_context].initialized,
                   "Cannot switch to uninitialized context when no initialized context exists.");
      m_contexts[idx].last = m_contexts[m_active_context].last;
      m_contexts[idx].initialized = true;
    }
    m_active_context = idx;
    return m_contexts[idx];
  }

 public:
  static constexpr int NUM_LAYERS = 2;

  using EncodedType = RGBNIRData;

  const RGBNIRData& last_value() const { return m_contexts[m_active_context].last; }

  explicit RGBNIR14Encoder(RGBNIRData initial, uint8_t context,
                           bool use_laszip_v3_context_quirk = false)
      : m_active_context(context), m_use_laszip_v3_context_quirk(use_laszip_v3_context_quirk) {
    m_contexts[context].last = initial;
    m_contexts[context].initialized = true;
  }

  // Layer 0 = RGB stream, Layer 1 = NIR stream
  RGBNIRData decode(LayeredInStreams<NUM_LAYERS>& streams, uint8_t ctx) {
    const uint8_t prev_active = m_active_context;
    const bool switched = (ctx != prev_active);
    const bool target_was_initialized = m_contexts[ctx].initialized;

    Context& c = ensure_context(ctx);
    RGBNIRData* base_last = &c.last;
    RGBNIRData* out_last = &c.last;
    if (m_use_laszip_v3_context_quirk && switched && target_was_initialized) {
      base_last = &m_contexts[prev_active].last;
      out_last = &m_contexts[prev_active].last;
    }
    InStream& rgb_stream = streams[0];
    InStream& nir_stream = streams[1];

    // Decode RGB (same algorithm as RGB14Encoder)
    uint_fast16_t sym = c.rgb_bytes_used.decode_symbol(rgb_stream);
    uint8_t r_lo = static_cast<uint8_t>(base_last->rgb.red);
    uint8_t r_hi = static_cast<uint8_t>(base_last->rgb.red >> 8);
    if (sym & (1 << 0)) {
      r_lo = static_cast<uint8_t>(r_lo + c.rgb_diff[0].decode_symbol(rgb_stream));
    }
    if (sym & (1 << 1)) {
      r_hi = static_cast<uint8_t>(r_hi + c.rgb_diff[1].decode_symbol(rgb_stream));
    }
    if (sym & (1 << 6)) {
      int d_lo = r_lo - static_cast<uint8_t>(base_last->rgb.red);
      int d_hi = r_hi - static_cast<uint8_t>(base_last->rgb.red >> 8);
      uint8_t g_lo = static_cast<uint8_t>(base_last->rgb.green);
      uint8_t g_hi = static_cast<uint8_t>(base_last->rgb.green >> 8);
      uint8_t b_lo = static_cast<uint8_t>(base_last->rgb.blue);
      uint8_t b_hi = static_cast<uint8_t>(base_last->rgb.blue >> 8);
      if (sym & (1 << 2)) {
        g_lo = static_cast<uint8_t>(clamp(g_lo, d_lo) + c.rgb_diff[2].decode_symbol(rgb_stream));
      }
      if (sym & (1 << 4)) {
        int d = (d_lo + static_cast<int>(g_lo) - (base_last->rgb.green & 0xFF)) / 2;
        b_lo = static_cast<uint8_t>(clamp(b_lo, d) + c.rgb_diff[4].decode_symbol(rgb_stream));
      }
      if (sym & (1 << 3)) {
        g_hi = static_cast<uint8_t>(clamp(g_hi, d_hi) + c.rgb_diff[3].decode_symbol(rgb_stream));
      }
      if (sym & (1 << 5)) {
        int d = (d_hi + static_cast<int>(g_hi) - (base_last->rgb.green >> 8)) / 2;
        b_hi = static_cast<uint8_t>(clamp(b_hi, d) + c.rgb_diff[5].decode_symbol(rgb_stream));
      }
      out_last->rgb.red = static_cast<uint16_t>(r_lo | (static_cast<uint16_t>(r_hi) << 8));
      out_last->rgb.green = static_cast<uint16_t>(g_lo | (static_cast<uint16_t>(g_hi) << 8));
      out_last->rgb.blue = static_cast<uint16_t>(b_lo | (static_cast<uint16_t>(b_hi) << 8));
    } else {
      out_last->rgb.red = static_cast<uint16_t>(r_lo | (static_cast<uint16_t>(r_hi) << 8));
      out_last->rgb.green = out_last->rgb.red;
      out_last->rgb.blue = out_last->rgb.red;
    }

    // Decode NIR
    uint_fast16_t nir_sym = c.nir_bytes_used.decode_symbol(nir_stream);
    uint8_t nir_lo = static_cast<uint8_t>(base_last->nir);
    uint8_t nir_hi = static_cast<uint8_t>(base_last->nir >> 8);
    if (nir_sym & (1 << 0)) {
      nir_lo = static_cast<uint8_t>(nir_lo + c.nir_diff[0].decode_symbol(nir_stream));
    }
    if (nir_sym & (1 << 1)) {
      nir_hi = static_cast<uint8_t>(nir_hi + c.nir_diff[1].decode_symbol(nir_stream));
    }
    out_last->nir = static_cast<uint16_t>(nir_lo | (static_cast<uint16_t>(nir_hi) << 8));

    return *out_last;
  }

  void encode(LayeredOutStreams<NUM_LAYERS>& streams, RGBNIRData value, uint8_t ctx) {
    const uint8_t prev_active = m_active_context;
    const bool switched = (ctx != prev_active);
    const bool target_was_initialized = m_contexts[ctx].initialized;

    Context& c = ensure_context(ctx);
    RGBNIRData* base_last = &c.last;
    RGBNIRData* out_last = &c.last;
    if (m_use_laszip_v3_context_quirk && switched && target_was_initialized) {
      base_last = &m_contexts[prev_active].last;
      out_last = &m_contexts[prev_active].last;
    }
    OutStream& rgb_stream = streams[0];
    OutStream& nir_stream = streams[1];

    // Encode RGB (same algorithm as RGB14Encoder)
    uint8_t r_lo = static_cast<uint8_t>(value.rgb.red);
    uint8_t r_hi = static_cast<uint8_t>(value.rgb.red >> 8);
    int d_lo = r_lo - static_cast<uint8_t>(base_last->rgb.red);
    int d_hi = r_hi - static_cast<uint8_t>(base_last->rgb.red >> 8);
    uint8_t g_lo = static_cast<uint8_t>(value.rgb.green);
    uint8_t g_hi = static_cast<uint8_t>(value.rgb.green >> 8);
    uint8_t b_lo = static_cast<uint8_t>(value.rgb.blue);
    uint8_t b_hi = static_cast<uint8_t>(value.rgb.blue >> 8);

    uint_fast16_t sym = (d_lo != 0 ? (1u << 0) : 0u);
    sym |= (d_hi != 0 ? (1u << 1) : 0u);
    bool colors_non_uniform = (g_lo != r_lo || g_hi != r_hi || b_lo != r_lo || b_hi != r_hi);
    sym |= (colors_non_uniform ? (1u << 6) : 0u);
    if (colors_non_uniform) {
      sym |= (g_lo != static_cast<uint8_t>(base_last->rgb.green) ? (1u << 2) : 0u);
      sym |= (g_hi != static_cast<uint8_t>(base_last->rgb.green >> 8) ? (1u << 3) : 0u);
      sym |= (b_lo != static_cast<uint8_t>(base_last->rgb.blue) ? (1u << 4) : 0u);
      sym |= (b_hi != static_cast<uint8_t>(base_last->rgb.blue >> 8) ? (1u << 5) : 0u);
    }
    c.rgb_bytes_used.encode_symbol(rgb_stream, sym);
    if (sym & (1 << 0)) {
      c.rgb_diff[0].encode_symbol(rgb_stream, static_cast<uint8_t>(d_lo));
    }
    if (sym & (1 << 1)) {
      c.rgb_diff[1].encode_symbol(rgb_stream, static_cast<uint8_t>(d_hi));
    }
    if (sym & (1 << 6)) {
      if (sym & (1 << 2)) {
        uint8_t base = clamp(static_cast<uint8_t>(base_last->rgb.green), d_lo);
        c.rgb_diff[2].encode_symbol(rgb_stream, static_cast<uint8_t>(g_lo - base));
      }
      if (sym & (1 << 4)) {
        int d = (d_lo + static_cast<int>(g_lo) - (base_last->rgb.green & 0xFF)) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(base_last->rgb.blue), d);
        c.rgb_diff[4].encode_symbol(rgb_stream, static_cast<uint8_t>(b_lo - base));
      }
      if (sym & (1 << 3)) {
        uint8_t base = clamp(static_cast<uint8_t>(base_last->rgb.green >> 8), d_hi);
        c.rgb_diff[3].encode_symbol(rgb_stream, static_cast<uint8_t>(g_hi - base));
      }
      if (sym & (1 << 5)) {
        int d = (d_hi + static_cast<int>(g_hi) - (base_last->rgb.green >> 8)) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(base_last->rgb.blue >> 8), d);
        c.rgb_diff[5].encode_symbol(rgb_stream, static_cast<uint8_t>(b_hi - base));
      }
    }

    // Encode NIR
    uint8_t nir_lo = static_cast<uint8_t>(value.nir);
    uint8_t nir_hi = static_cast<uint8_t>(value.nir >> 8);
    uint_fast16_t nir_sym = (nir_lo != static_cast<uint8_t>(base_last->nir) ? (1u << 0) : 0u);
    nir_sym |= (nir_hi != static_cast<uint8_t>(base_last->nir >> 8) ? (1u << 1) : 0u);
    c.nir_bytes_used.encode_symbol(nir_stream, nir_sym);
    if (nir_sym & (1 << 0)) {
      c.nir_diff[0].encode_symbol(
          nir_stream, static_cast<uint8_t>(nir_lo - static_cast<uint8_t>(base_last->nir)));
    }
    if (nir_sym & (1 << 1)) {
      c.nir_diff[1].encode_symbol(
          nir_stream, static_cast<uint8_t>(nir_hi - static_cast<uint8_t>(base_last->nir >> 8)));
    }

    *out_last = value;
  }
};

}  // namespace laspp
