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

#include "las_point.hpp"
#include "laz/layered_stream.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

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

  static uint8_t clamp(uint8_t value, int delta) {
    if (delta + value > 255) {
      return 255;
    } else if (delta + value < 0) {
      return 0;
    }
    return static_cast<uint8_t>(value + delta);
  }

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

  explicit RGB14Encoder(ColorData initial_color_data, uint8_t context) : m_active_context(context) {
    m_contexts[context].last_value = initial_color_data;
    m_contexts[context].initialized = true;
  }

  ColorData decode(LayeredInStreams<NUM_LAYERS>& in_streams, uint8_t context_idx) {
    InStream& in_stream = in_streams[0];
    Context& context = ensure_context(context_idx);
    std::cout << "Decoding with context " << static_cast<int>(context_idx) << std::endl;

    uint_fast16_t changed_values = context.changed_values_encoder.decode_symbol(in_stream);

    uint8_t red_low = static_cast<uint8_t>(context.last_value.red);
    uint8_t red_high = static_cast<uint8_t>(context.last_value.red >> 8);

    if (changed_values & 1) {
      uint8_t delta = static_cast<uint8_t>(context.red_low_encoder.decode_symbol(in_stream));
      red_low = static_cast<uint8_t>((red_low + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
    }
    if (changed_values & (1 << 1)) {
      uint8_t delta = static_cast<uint8_t>(context.red_high_encoder.decode_symbol(in_stream));
      red_high =
          static_cast<uint8_t>((red_high + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
    }

    ColorData decoded_value{};
    decoded_value.red = static_cast<uint16_t>(
        (static_cast<uint16_t>(red_low) | (static_cast<uint16_t>(red_high) << 8)));

    if (changed_values &
        (1 << 6)) {  // Seems to be opposite of the specification but consistent with LASzip
      uint8_t green_low = static_cast<uint8_t>(context.last_value.green);
      uint8_t green_high = static_cast<uint8_t>(context.last_value.green >> 8);
      uint8_t blue_low = static_cast<uint8_t>(context.last_value.blue);
      uint8_t blue_high = static_cast<uint8_t>(context.last_value.blue >> 8);

      int d_red_low = red_low - static_cast<uint8_t>(context.last_value.red);
      int d_red_high = red_high - static_cast<uint8_t>(context.last_value.red >> 8);

      if (changed_values & (1 << 2)) {
        uint8_t base = clamp(green_low, d_red_low);
        uint8_t delta = static_cast<uint8_t>(context.green_low_encoder.decode_symbol(in_stream));
        green_low = static_cast<uint8_t>((base + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
      }
      if (changed_values &
          (1 << 4)) {  // This ordering is inconsistent with the specification but matches LASzip
        int d_green_low = green_low - static_cast<uint8_t>(context.last_value.green);
        int d = (d_red_low + d_green_low) / 2;
        uint8_t base = clamp(blue_low, d);
        uint8_t delta = static_cast<uint8_t>(context.blue_low_encoder.decode_symbol(in_stream));
        blue_low = static_cast<uint8_t>((base + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
      }
      if (changed_values & (1 << 3)) {
        uint8_t base = clamp(green_high, d_red_high);
        uint8_t delta = static_cast<uint8_t>(context.green_high_encoder.decode_symbol(in_stream));
        green_high =
            static_cast<uint8_t>((base + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
      }
      if (changed_values & (1 << 5)) {
        int d_green_high = green_high - static_cast<uint8_t>(context.last_value.green >> 8);
        int d = (d_red_high + d_green_high) / 2;
        uint8_t base = clamp(blue_high, d);
        uint8_t delta = static_cast<uint8_t>(context.blue_high_encoder.decode_symbol(in_stream));
        blue_high = static_cast<uint8_t>((base + delta) & 0xFF);  // Match U8_FOLD wrapping behavior
      }

      decoded_value.green = static_cast<uint16_t>(
          (static_cast<uint16_t>(green_low) | (static_cast<uint16_t>(green_high) << 8)));
      decoded_value.blue = static_cast<uint16_t>(
          (static_cast<uint16_t>(blue_low) | (static_cast<uint16_t>(blue_high) << 8)));
    } else {
      decoded_value.green = decoded_value.red;
      decoded_value.blue = decoded_value.red;
    }

    context.last_value = decoded_value;
    return context.last_value;
  }

  void encode(LayeredOutStreams<NUM_LAYERS>& out_streams, ColorData color_data,
              uint8_t context_idx) {
    OutStream& out_stream = out_streams[0];
    Context& context = ensure_context(context_idx);

    uint8_t red_low = static_cast<uint8_t>(color_data.red);
    uint8_t red_high = static_cast<uint8_t>(color_data.red >> 8);
    int d_red_low = red_low - static_cast<uint8_t>(context.last_value.red);
    int d_red_high = red_high - static_cast<uint8_t>(context.last_value.red >> 8);
    uint8_t green_low = static_cast<uint8_t>(color_data.green);
    uint8_t green_high = static_cast<uint8_t>(color_data.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(color_data.blue);
    uint8_t blue_high = static_cast<uint8_t>(color_data.blue >> 8);

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
      if (green_low != static_cast<uint8_t>(context.last_value.green)) {
        changed_values |= (1 << 2);
      }
      if (green_high != static_cast<uint8_t>(context.last_value.green >> 8)) {
        changed_values |= (1 << 3);
      }
      if (blue_low != static_cast<uint8_t>(context.last_value.blue)) {
        changed_values |= (1 << 4);
      }
      if (blue_high != static_cast<uint8_t>(context.last_value.blue >> 8)) {
        changed_values |= (1 << 5);
      }
    }

    context.changed_values_encoder.encode_symbol(out_stream, changed_values);
    if (changed_values & 1) {
      context.red_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_low));
    }
    if (changed_values & (1 << 1)) {
      context.red_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_high));
    }
    if (changed_values & (1 << 6)) {
      if (changed_values & (1 << 2)) {
        uint8_t base = clamp(static_cast<uint8_t>(context.last_value.green), d_red_low);
        context.green_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_low - base));
      }
      if (changed_values & (1 << 4)) {
        int d = (d_red_low + (green_low - static_cast<uint8_t>(context.last_value.green))) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(context.last_value.blue), d);
        context.blue_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_low - base));
      }
      if (changed_values & (1 << 3)) {
        uint8_t base = clamp(static_cast<uint8_t>(context.last_value.green >> 8), d_red_high);
        context.green_high_encoder.encode_symbol(out_stream,
                                                 static_cast<uint8_t>(green_high - base));
      }
      if (changed_values & (1 << 5)) {
        int d =
            (d_red_high + (green_high - static_cast<uint8_t>(context.last_value.green >> 8))) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(context.last_value.blue >> 8), d);
        context.blue_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_high - base));
      }
    }

    context.last_value = color_data;
  }
};

}  // namespace laspp
