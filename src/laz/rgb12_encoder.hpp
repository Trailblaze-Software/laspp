/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
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

#include "las_point.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/arithmetic.hpp"
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

 public:
  using EncodedType = ColorData;
  const EncodedType& last_value() const { return m_last_value; }

  explicit RGB12Encoder(ColorData initial_color_data) : m_last_value(initial_color_data) {}

 private:
  void decode_red_channels(InStream& in_stream, uint_fast16_t changed_values, uint8_t& red_low,
                           uint8_t& red_high) {
    if (changed_values & 1) {
      red_low += static_cast<uint8_t>(m_red_low_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1u << 1u)) {
      red_high += static_cast<uint8_t>(m_red_high_encoder.decode_symbol(in_stream));
    }
  }

  void decode_green_blue_channels(InStream& in_stream, uint_fast16_t changed_values, int d_red_low,
                                  int d_red_high) {
    uint8_t green_low = static_cast<uint8_t>(m_last_value.green);
    uint8_t green_high = static_cast<uint8_t>(m_last_value.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(m_last_value.blue);
    uint8_t blue_high = static_cast<uint8_t>(m_last_value.blue >> 8);

    if (changed_values & (1u << 2u)) {
      green_low = clamp(green_low, d_red_low) +
                  static_cast<uint8_t>(m_green_low_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1u << 3u)) {
      green_high = clamp(green_high, d_red_high) +
                   static_cast<uint8_t>(m_green_high_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1u << 4u)) {
      int d_green_low = green_low - static_cast<uint8_t>(m_last_value.green);
      int d = (d_red_low + d_green_low) / 2;
      blue_low =
          clamp(blue_low, d) + static_cast<uint8_t>(m_blue_low_encoder.decode_symbol(in_stream));
    }
    if (changed_values & (1u << 5u)) {
      int d_green_high = green_high - static_cast<uint8_t>(m_last_value.green >> 8);
      int d = (d_red_high + d_green_high) / 2;
      blue_high =
          clamp(blue_high, d) + static_cast<uint8_t>(m_blue_high_encoder.decode_symbol(in_stream));
    }

    m_last_value.green = static_cast<uint16_t>(green_low | static_cast<uint16_t>(green_high << 8u));
    m_last_value.blue = static_cast<uint16_t>(blue_low | static_cast<uint16_t>(blue_high << 8u));
  }

 public:
  ColorData decode(InStream& in_stream) {
    uint_fast16_t changed_values = m_changed_values_encoder.decode_symbol(in_stream);
    uint8_t red_low = static_cast<uint8_t>(m_last_value.red);
    uint8_t red_high = static_cast<uint8_t>(m_last_value.red >> 8);

    decode_red_channels(in_stream, changed_values, red_low, red_high);

    if (changed_values & (1u << 6u)) {
      m_last_value.red = static_cast<uint16_t>(red_low | static_cast<uint16_t>(red_high << 8u));
      m_last_value.green = m_last_value.red;
      m_last_value.blue = m_last_value.red;
    } else {
      int d_red_low = red_low - static_cast<uint8_t>(m_last_value.red);
      int d_red_high = red_high - static_cast<uint8_t>(m_last_value.red >> 8u);
      decode_green_blue_channels(in_stream, changed_values, d_red_low, d_red_high);
      m_last_value.red = static_cast<uint16_t>(red_low | static_cast<uint16_t>(red_high << 8u));
    }

    return m_last_value;
  }

 private:
  uint_fast16_t compute_changed_values(uint8_t red_low, uint8_t red_high, uint8_t green_low,
                                       uint8_t green_high, uint8_t blue_low, uint8_t blue_high,
                                       int d_red_low, int d_red_high) {
    uint_fast16_t changed_values = 0;

    if (d_red_low != 0) {
      changed_values |= 1;
    }
    if (d_red_high != 0) {
      changed_values |= (1u << 1u);
    }
    if (green_low == red_low && green_high == red_high && blue_low == red_low &&
        blue_high == red_high) {
      changed_values |= (1u << 6u);
    } else {
      if (green_low != static_cast<uint8_t>(m_last_value.green)) {
        changed_values |= (1u << 2u);
      }
      if (green_high != static_cast<uint8_t>(m_last_value.green >> 8)) {
        changed_values |= (1u << 3u);
      }
      if (blue_low != static_cast<uint8_t>(m_last_value.blue)) {
        changed_values |= (1u << 4u);
      }
      if (blue_high != static_cast<uint8_t>(m_last_value.blue >> 8)) {
        changed_values |= (1u << 5u);
      }
    }
    return changed_values;
  }

  void encode_green_blue_channels(OutStream& out_stream, uint_fast16_t changed_values,
                                  uint8_t green_low, uint8_t green_high, uint8_t blue_low,
                                  uint8_t blue_high, int d_red_low, int d_red_high) {
    if (changed_values & (1u << 2u)) {
      uint8_t base = clamp(static_cast<uint8_t>(m_last_value.green), d_red_low);
      m_green_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_low - base));
    }
    if (changed_values & (1u << 3u)) {
      uint8_t base = clamp(static_cast<uint8_t>(m_last_value.green >> 8), d_red_high);
      m_green_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(green_high - base));
    }
    if (changed_values & (1u << 4u)) {
      int d = (d_red_low + (green_low - static_cast<uint8_t>(m_last_value.green))) / 2;
      uint8_t base = clamp(static_cast<uint8_t>(m_last_value.blue), d);
      m_blue_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_low - base));
    }
    if (changed_values & (1u << 5u)) {
      int d = (d_red_high + (green_high - static_cast<uint8_t>(m_last_value.green >> 8))) / 2;
      uint8_t base = clamp(static_cast<uint8_t>(m_last_value.blue >> 8), d);
      m_blue_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(blue_high - base));
    }
  }

 public:
  void encode(OutStream& out_stream, ColorData color_data) {
    uint8_t red_low = static_cast<uint8_t>(color_data.red);
    uint8_t red_high = static_cast<uint8_t>(color_data.red >> 8);
    int d_red_low = red_low - static_cast<uint8_t>(m_last_value.red);
    int d_red_high = red_high - static_cast<uint8_t>(m_last_value.red >> 8);
    uint8_t green_low = static_cast<uint8_t>(color_data.green);
    uint8_t green_high = static_cast<uint8_t>(color_data.green >> 8);
    uint8_t blue_low = static_cast<uint8_t>(color_data.blue);
    uint8_t blue_high = static_cast<uint8_t>(color_data.blue >> 8);

    uint_fast16_t changed_values = compute_changed_values(
        red_low, red_high, green_low, green_high, blue_low, blue_high, d_red_low, d_red_high);

    m_changed_values_encoder.encode_symbol(out_stream, changed_values);
    if (changed_values & 1) {
      m_red_low_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_low));
    }
    if (changed_values & (1u << 1u)) {
      m_red_high_encoder.encode_symbol(out_stream, static_cast<uint8_t>(d_red_high));
    }
    if (!(changed_values & (1u << 6u))) {
      encode_green_blue_channels(out_stream, changed_values, green_low, green_high, blue_low,
                                 blue_high, d_red_low, d_red_high);
    }
    m_last_value = color_data;
  }
};

}  // namespace laspp
