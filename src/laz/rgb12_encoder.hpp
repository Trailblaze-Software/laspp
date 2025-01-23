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

#include <bitset>

#include "las_point.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
namespace laspp {

class RGB12Encoder {
  ColorData m_last_value;

  SymbolEncoder<128> m_changed_values_encoder;
  SymbolEncoder<256> m_red_low_encoder;
  SymbolEncoder<256> m_red_high_encoder;
  SymbolEncoder<256> m_green_low_encoder;
  SymbolEncoder<256> m_green_high_encoder;
  SymbolEncoder<256> m_blue_low_encoder;
  SymbolEncoder<256> m_blue_high_encoder;

  static uint8_t clamp(uint8_t value, int delta) {
    if (delta + value > 255) {
      return 255;
    } else if (delta + value < 0) {
      return 0;
    }
    return static_cast<uint8_t>(value + delta);
  }

 public:
  using EncodedType = ColorData;
  const EncodedType& last_value() const { return m_last_value; }

  explicit RGB12Encoder(ColorData initial_color_data) : m_last_value(initial_color_data) {}

  ColorData decode(InStream& in_stream) {
    uint_fast16_t changed_values = m_changed_values_encoder.decode_symbol(in_stream);
    uint8_t red_low = static_cast<uint8_t>(m_last_value.red);
    uint8_t red_high = static_cast<uint8_t>(m_last_value.red >> 8);
    if (changed_values & 1) {
      red_low += static_cast<uint8_t>(m_red_low_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1 << 1)) {
      red_high += static_cast<uint8_t>(m_red_high_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1 << 6)) {
      m_last_value.red = red_low | static_cast<uint16_t>(red_high << 8);
      m_last_value.green = m_last_value.red;
      m_last_value.blue = m_last_value.red;
    } else {
      int d_red_low = red_low - static_cast<uint8_t>(m_last_value.red);
      int d_red_high = red_high - static_cast<uint8_t>(m_last_value.red >> 8);
      uint8_t green_low = static_cast<uint8_t>(m_last_value.green);
      uint8_t green_high = static_cast<uint8_t>(m_last_value.green >> 8);
      uint8_t blue_low = static_cast<uint8_t>(m_last_value.blue);
      uint8_t blue_high = static_cast<uint8_t>(m_last_value.blue >> 8);
      if (changed_values & (1 << 2)) {
        green_low = clamp(green_low, d_red_low) +
                    static_cast<uint8_t>(m_green_low_encoder.decode_symbol(in_stream));
      }
      if (changed_values & (1 << 3)) {
        green_high = clamp(green_high, d_red_high) +
                     static_cast<uint8_t>(m_green_high_encoder.decode_symbol(in_stream));
      }
      if (changed_values & (1 << 4)) {
        int d_green_low = green_low - static_cast<uint8_t>(m_last_value.green);
        int d = (d_red_low + d_green_low) / 2;
        blue_low =
            clamp(blue_low, d) + static_cast<uint8_t>(m_blue_low_encoder.decode_symbol(in_stream));
      }
      if (changed_values & (1 << 5)) {
        int d_green_high = green_high - static_cast<uint8_t>(m_last_value.green >> 8);
        int d = (d_red_high + d_green_high) / 2;
        blue_high = clamp(blue_high, d) +
                    static_cast<uint8_t>(m_blue_high_encoder.decode_symbol(in_stream));
      }
      m_last_value.red = red_low | static_cast<uint16_t>(red_high << 8);
      m_last_value.green = green_low | static_cast<uint16_t>(green_high << 8);
      m_last_value.blue = blue_low | static_cast<uint16_t>(blue_high << 8);
    }

    return m_last_value;
  }

  void encode(OutStream& out_stream, ColorData color_data) {
    uint_fast16_t changed_values = 0;

    uint8_t red_low = static_cast<uint8_t>(color_data.red);
    uint8_t red_high = static_cast<uint8_t>(color_data.red >> 8);
    int d_red_low = red_low - static_cast<uint8_t>(m_last_value.red);
    int d_red_high = red_high - static_cast<uint8_t>(m_last_value.red >> 8);
    uint8_t green_low = static_cast<uint8_t>(color_data.green);
    uint8_t green_high = static_cast<uint8_t>(color_data.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(color_data.blue);
    uint8_t blue_high = static_cast<uint8_t>(color_data.blue >> 8);

    if (d_red_low != 0) {
      changed_values |= 1;
    }
    if (d_red_high != 0) {
      changed_values |= (1 << 1);
    }
    if (green_low == red_low && green_high == red_high && blue_low == red_low &&
        blue_high == red_high) {
      changed_values |= (1 << 6);
    } else {
      if (green_low != static_cast<uint8_t>(m_last_value.green)) {
        changed_values |= (1 << 2);
      }
      if (green_high != static_cast<uint8_t>(m_last_value.green >> 8)) {
        changed_values |= (1 << 3);
      }
      if (blue_low != static_cast<uint8_t>(m_last_value.blue)) {
        changed_values |= (1 << 4);
      }
      if (blue_high != static_cast<uint8_t>(m_last_value.blue >> 8)) {
        changed_values |= (1 << 5);
      }
    }
    m_changed_values_encoder.encode_symbol(out_stream, changed_values);
    if (changed_values & 1) {
      m_red_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_low));
    }
    if (changed_values & (1 << 1)) {
      m_red_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_high));
    }
    if (!(changed_values & (1 << 6))) {
      if (changed_values & (1 << 2)) {
        uint8_t base = clamp(static_cast<uint8_t>(m_last_value.green), d_red_low);
        m_green_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_low - base));
      }
      if (changed_values & (1 << 3)) {
        uint8_t base = clamp(static_cast<uint8_t>(m_last_value.green >> 8), d_red_high);
        m_green_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_high - base));
      }
      if (changed_values & (1 << 4)) {
        int d = (d_red_low + (green_low - static_cast<uint8_t>(m_last_value.green))) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(m_last_value.blue), d);
        m_blue_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_low - base));
      }
      if (changed_values & (1 << 5)) {
        int d = (d_red_high + (green_high - static_cast<uint8_t>(m_last_value.green >> 8))) / 2;
        uint8_t base = clamp(static_cast<uint8_t>(m_last_value.blue >> 8), d);
        m_blue_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_high - base));
      }
    }
    m_last_value = color_data;
  }
};

}  // namespace laspp
