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

class RGB14Encoder {
  struct Context {
    bool initialized;
    ColorData last_value;
    SymbolEncoder<1 << 7> changed_values_encoder;
    SymbolEncoder<256> red_low_encoder;
    SymbolEncoder<256> red_high_encoder;
    SymbolEncoder<256> green_low_encoder;
    SymbolEncoder<256> green_high_encoder;
    SymbolEncoder<256> blue_low_encoder;
    SymbolEncoder<256> blue_high_encoder;

    Context() : initialized(false) {}
  };

  std::array<Context, 4> m_contexts;
  uint8_t m_active_context;
  bool m_use_laszip_v3_context_quirk = false;

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
  static constexpr int NUM_LAYERS = 1;

  using EncodedType = ColorData;

  const EncodedType& last_value() const { return m_contexts[m_active_context].last_value; }

  explicit RGB14Encoder(ColorData initial_color_data, uint8_t context,
                        bool use_laszip_v3_context_quirk = false)
      : m_active_context(context), m_use_laszip_v3_context_quirk(use_laszip_v3_context_quirk) {
    m_contexts[context].last_value = initial_color_data;
    m_contexts[context].initialized = true;
  }

 private:
  void decode_red_channels(Context& context, InStream& in_stream, uint_fast16_t changed_values,
                           uint8_t& red_low, uint8_t& red_high) {
    if (changed_values & 1) {
      uint8_t delta = static_cast<uint8_t>(context.red_low_encoder.decode_symbol(in_stream));
      red_low = static_cast<uint8_t>(red_low + delta);
    }
    if (changed_values & (1 << 1)) {
      uint8_t delta = static_cast<uint8_t>(context.red_high_encoder.decode_symbol(in_stream));
      red_high = static_cast<uint8_t>(red_high + delta);
    }
  }

  void decode_green_blue_channels(Context& context, InStream& in_stream,
                                  uint_fast16_t changed_values, ColorData& last_value,
                                  uint8_t red_low, uint8_t red_high) {
    uint8_t green_low = static_cast<uint8_t>(last_value.green);
    uint8_t green_high = static_cast<uint8_t>(last_value.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(last_value.blue);
    uint8_t blue_high = static_cast<uint8_t>(last_value.blue >> 8);

    int d_red_low = red_low - static_cast<uint8_t>(last_value.red);
    int d_red_high = red_high - static_cast<uint8_t>(last_value.red >> 8);

    if (changed_values & (1 << 2)) {
      uint8_t base = clamp(green_low, d_red_low);
      uint8_t delta = static_cast<uint8_t>(context.green_low_encoder.decode_symbol(in_stream));
      green_low = static_cast<uint8_t>((base + delta) & 0xFF);
    }
    if (changed_values & (1 << 4)) {
      int d_green_low = green_low - static_cast<uint8_t>(last_value.green);
      int d = (d_red_low + d_green_low) / 2;
      uint8_t base = clamp(blue_low, d);
      uint8_t delta = static_cast<uint8_t>(context.blue_low_encoder.decode_symbol(in_stream));
      blue_low = static_cast<uint8_t>((base + delta) & 0xFF);
    }
    if (changed_values & (1 << 3)) {
      uint8_t base = clamp(green_high, d_red_high);
      uint8_t delta = static_cast<uint8_t>(context.green_high_encoder.decode_symbol(in_stream));
      green_high = static_cast<uint8_t>((base + delta) & 0xFF);
    }
    if (changed_values & (1 << 5)) {
      int d_green_high = green_high - static_cast<uint8_t>(last_value.green >> 8);
      int d = (d_red_high + d_green_high) / 2;
      uint8_t base = clamp(blue_high, d);
      uint8_t delta = static_cast<uint8_t>(context.blue_high_encoder.decode_symbol(in_stream));
      blue_high = static_cast<uint8_t>((base + delta) & 0xFF);
    }

    last_value.green = static_cast<uint16_t>(
        (static_cast<uint16_t>(green_low) | (static_cast<uint16_t>(green_high) << 8)));
    last_value.blue = static_cast<uint16_t>(
        (static_cast<uint16_t>(blue_low) | (static_cast<uint16_t>(blue_high) << 8)));
  }

 public:
  ColorData decode(LayeredInStreams<NUM_LAYERS>& in_streams, uint8_t context_idx) {
    // LASzip only emits RGB layer bytes when at least one RGB value changed in the chunk.
    // If the layer is empty, we must not advance the arithmetic decoder state; just reuse
    // the previous value.
    if (!in_streams.non_empty(0)) {
      Context& context = ensure_context(context_idx);
      return context.last_value;
    }

    InStream& in_stream = in_streams[0];
    const uint8_t prev_active = m_active_context;
    const bool switched = (context_idx != prev_active);
    const bool target_was_initialized = m_contexts[context_idx].initialized;

    Context& context = ensure_context(context_idx);
    ColorData* base_last = &context.last_value;
    ColorData* out_last = &context.last_value;
    if (m_use_laszip_v3_context_quirk && switched && target_was_initialized) {
      base_last = &m_contexts[prev_active].last_value;
      out_last = &m_contexts[prev_active].last_value;
    }
    ColorData working = *base_last;
    uint_fast16_t changed_values = context.changed_values_encoder.decode_symbol(in_stream);

    uint8_t red_low = static_cast<uint8_t>(working.red);
    uint8_t red_high = static_cast<uint8_t>(working.red >> 8);

    decode_red_channels(context, in_stream, changed_values, red_low, red_high);

    if (changed_values & (1 << 6)) {
      decode_green_blue_channels(context, in_stream, changed_values, working, red_low, red_high);
      working.red = static_cast<uint16_t>(
          (static_cast<uint16_t>(red_low) | (static_cast<uint16_t>(red_high) << 8)));
    } else {
      working.red = static_cast<uint16_t>(
          (static_cast<uint16_t>(red_low) | (static_cast<uint16_t>(red_high) << 8)));
      working.green = working.red;
      working.blue = working.red;
    }

    *out_last = working;
    return working;
  }

 private:
  uint_fast16_t compute_changed_values(const ColorData& last_value, uint8_t red_low,
                                       uint8_t red_high, uint8_t green_low, uint8_t green_high,
                                       uint8_t blue_low, uint8_t blue_high, int d_red_low,
                                       int d_red_high) {
    uint_fast16_t changed_values = 0;

    if (d_red_low != 0) {
      changed_values |= 1;
    }
    if (d_red_high != 0) {
      changed_values |= (1 << 1);
    }
    if (green_low != red_low || green_high != red_high || blue_low != red_low ||
        blue_high != red_high) {
      changed_values |= (1 << 6);
      if (green_low != static_cast<uint8_t>(last_value.green)) {
        changed_values |= (1 << 2);
      }
      if (green_high != static_cast<uint8_t>(last_value.green >> 8)) {
        changed_values |= (1 << 3);
      }
      if (blue_low != static_cast<uint8_t>(last_value.blue)) {
        changed_values |= (1 << 4);
      }
      if (blue_high != static_cast<uint8_t>(last_value.blue >> 8)) {
        changed_values |= (1 << 5);
      }
    }
    return changed_values;
  }

  void encode_green_blue_channels(Context& context, OutStream& out_stream,
                                  uint_fast16_t changed_values, const ColorData& last_value,
                                  uint8_t green_low, uint8_t green_high, uint8_t blue_low,
                                  uint8_t blue_high, int d_red_low, int d_red_high) {
    if (changed_values & (1 << 2)) {
      uint8_t base = clamp(static_cast<uint8_t>(last_value.green), d_red_low);
      context.green_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_low - base));
    }
    if (changed_values & (1 << 4)) {
      int d = (d_red_low + (green_low - static_cast<uint8_t>(last_value.green))) / 2;
      uint8_t base = clamp(static_cast<uint8_t>(last_value.blue), d);
      context.blue_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_low - base));
    }
    if (changed_values & (1 << 3)) {
      uint8_t base = clamp(static_cast<uint8_t>(last_value.green >> 8), d_red_high);
      context.green_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_high - base));
    }
    if (changed_values & (1 << 5)) {
      int d = (d_red_high + (green_high - static_cast<uint8_t>(last_value.green >> 8))) / 2;
      uint8_t base = clamp(static_cast<uint8_t>(last_value.blue >> 8), d);
      context.blue_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_high - base));
    }
  }

 public:
  void encode(LayeredOutStreams<NUM_LAYERS>& out_streams, ColorData color_data,
              uint8_t context_idx) {
    OutStream& out_stream = out_streams[0];
    const uint8_t prev_active = m_active_context;
    const bool switched = (context_idx != prev_active);
    const bool target_was_initialized = m_contexts[context_idx].initialized;

    Context& context = ensure_context(context_idx);

    ColorData* base_last = &context.last_value;
    ColorData* out_last = &context.last_value;
    if (m_use_laszip_v3_context_quirk && switched && target_was_initialized) {
      base_last = &m_contexts[prev_active].last_value;
      out_last = &m_contexts[prev_active].last_value;
    }
    const ColorData& last_value = *base_last;

    uint8_t red_low = static_cast<uint8_t>(color_data.red);
    uint8_t red_high = static_cast<uint8_t>(color_data.red >> 8);
    int d_red_low = red_low - static_cast<uint8_t>(last_value.red);
    int d_red_high = red_high - static_cast<uint8_t>(last_value.red >> 8);
    uint8_t green_low = static_cast<uint8_t>(color_data.green);
    uint8_t green_high = static_cast<uint8_t>(color_data.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(color_data.blue);
    uint8_t blue_high = static_cast<uint8_t>(color_data.blue >> 8);

    uint_fast16_t changed_values =
        compute_changed_values(last_value, red_low, red_high, green_low, green_high, blue_low,
                               blue_high, d_red_low, d_red_high);

    context.changed_values_encoder.encode_symbol(out_stream, changed_values);
    if (changed_values & 1) {
      context.red_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_low));
    }
    if (changed_values & (1 << 1)) {
      context.red_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_high));
    }
    if (changed_values & (1 << 6)) {
      encode_green_blue_channels(context, out_stream, changed_values, last_value, green_low,
                                 green_high, blue_low, blue_high, d_red_low, d_red_high);
    }

    *out_last = color_data;
  }
};

}  // namespace laspp
