/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "las_point.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/arithmetic.hpp"

namespace laspp {

enum class RGB12Version : uint8_t { V1 = 1, V2 = 2 };

template <RGB12Version V>
class RGB12EncoderT {
  static constexpr uint16_t kMaskAlphabet = (V == RGB12Version::V2) ? 128 : 64;

  ColorData m_last_value;

  // v1: 6-bit byte mask (0..5), each changed byte stores "delta from last byte".
  // v2: 7-bit mask (0..6), bit 6 indicates non-gray; bytes 2..5 use correlated predictors.
  SymbolEncoder<kMaskAlphabet> m_byte_used_encoder;

  std::array<SymbolEncoder<256>, 6> m_diff{};

 public:
  using EncodedType = ColorData;

  explicit RGB12EncoderT(ColorData initial_color_data) : m_last_value(initial_color_data) {}

  const EncodedType& last_value() const { return m_last_value; }

 private:
  static void to_bytes(const ColorData& c, uint8_t out[6]) {
    out[0] = static_cast<uint8_t>(c.red);
    out[1] = static_cast<uint8_t>(c.red >> 8);
    out[2] = static_cast<uint8_t>(c.green);
    out[3] = static_cast<uint8_t>(c.green >> 8);
    out[4] = static_cast<uint8_t>(c.blue);
    out[5] = static_cast<uint8_t>(c.blue >> 8);
  }

  static ColorData from_bytes(const uint8_t in[6]) {
    ColorData c{};
    c.red = static_cast<uint16_t>(in[0] | static_cast<uint16_t>(in[1] << 8u));
    c.green = static_cast<uint16_t>(in[2] | static_cast<uint16_t>(in[3] << 8u));
    c.blue = static_cast<uint16_t>(in[4] | static_cast<uint16_t>(in[5] << 8u));
    return c;
  }

 public:
  ColorData decode(InStream& in_stream) {
    uint8_t last[6];
    to_bytes(m_last_value, last);

    uint8_t cur[6];
    std::memcpy(cur, last, 6);

    const uint32_t sym = static_cast<uint32_t>(m_byte_used_encoder.decode_symbol(in_stream));

    if (sym & (1u << 0u))
      cur[0] =
          static_cast<uint8_t>(last[0] + static_cast<uint8_t>(m_diff[0].decode_symbol(in_stream)));
    if (sym & (1u << 1u))
      cur[1] =
          static_cast<uint8_t>(last[1] + static_cast<uint8_t>(m_diff[1].decode_symbol(in_stream)));

    if constexpr (V == RGB12Version::V2) {
      if (sym & (1u << 6u)) {
        int diff_l = static_cast<int>(cur[0]) - static_cast<int>(last[0]);
        if (sym & (1u << 2u)) {
          const uint8_t corr = static_cast<uint8_t>(m_diff[2].decode_symbol(in_stream));
          cur[2] = static_cast<uint8_t>(corr + clamp(last[2], diff_l));
        }
        if (sym & (1u << 4u)) {
          const uint8_t corr = static_cast<uint8_t>(m_diff[4].decode_symbol(in_stream));
          diff_l = (diff_l + (static_cast<int>(cur[2]) - static_cast<int>(last[2]))) / 2;
          cur[4] = static_cast<uint8_t>(corr + clamp(last[4], diff_l));
        }

        int diff_h = static_cast<int>(cur[1]) - static_cast<int>(last[1]);
        if (sym & (1u << 3u)) {
          const uint8_t corr = static_cast<uint8_t>(m_diff[3].decode_symbol(in_stream));
          cur[3] = static_cast<uint8_t>(corr + clamp(last[3], diff_h));
        }
        if (sym & (1u << 5u)) {
          const uint8_t corr = static_cast<uint8_t>(m_diff[5].decode_symbol(in_stream));
          diff_h = (diff_h + (static_cast<int>(cur[3]) - static_cast<int>(last[3]))) / 2;
          cur[5] = static_cast<uint8_t>(corr + clamp(last[5], diff_h));
        }
      } else {
        // grayscale: copy red into green/blue
        cur[2] = cur[0];
        cur[3] = cur[1];
        cur[4] = cur[0];
        cur[5] = cur[1];
      }
    } else {
      // v1: independent bytes (no predictors)
      if (sym & (1u << 2u))
        cur[2] = static_cast<uint8_t>(last[2] +
                                      static_cast<uint8_t>(m_diff[2].decode_symbol(in_stream)));
      if (sym & (1u << 3u))
        cur[3] = static_cast<uint8_t>(last[3] +
                                      static_cast<uint8_t>(m_diff[3].decode_symbol(in_stream)));
      if (sym & (1u << 4u))
        cur[4] = static_cast<uint8_t>(last[4] +
                                      static_cast<uint8_t>(m_diff[4].decode_symbol(in_stream)));
      if (sym & (1u << 5u))
        cur[5] = static_cast<uint8_t>(last[5] +
                                      static_cast<uint8_t>(m_diff[5].decode_symbol(in_stream)));
    }

    m_last_value = from_bytes(cur);
    return m_last_value;
  }

  void encode(OutStream& out_stream, ColorData color_data) {
    uint8_t last[6];
    to_bytes(m_last_value, last);
    uint8_t cur[6];
    to_bytes(color_data, cur);

    uint32_t sym = 0;
    sym |= (last[0] != cur[0]) ? (1u << 0u) : 0u;
    sym |= (last[1] != cur[1]) ? (1u << 1u) : 0u;
    sym |= (last[2] != cur[2]) ? (1u << 2u) : 0u;
    sym |= (last[3] != cur[3]) ? (1u << 3u) : 0u;
    sym |= (last[4] != cur[4]) ? (1u << 4u) : 0u;
    sym |= (last[5] != cur[5]) ? (1u << 5u) : 0u;

    if constexpr (V == RGB12Version::V2) {
      const bool non_gray =
          (cur[0] != cur[2]) || (cur[0] != cur[4]) || (cur[1] != cur[3]) || (cur[1] != cur[5]);
      sym |= non_gray ? (1u << 6u) : 0u;
    }

    m_byte_used_encoder.encode_symbol(out_stream, sym);

    int diff_l = 0;
    int diff_h = 0;

    if (sym & (1u << 0u)) {
      diff_l = static_cast<int>(cur[0]) - static_cast<int>(last[0]);
      m_diff[0].encode_symbol(out_stream, static_cast<uint8_t>(diff_l));
    }
    if (sym & (1u << 1u)) {
      diff_h = static_cast<int>(cur[1]) - static_cast<int>(last[1]);
      m_diff[1].encode_symbol(out_stream, static_cast<uint8_t>(diff_h));
    }

    if constexpr (V == RGB12Version::V2) {
      if (sym & (1u << 6u)) {
        if (sym & (1u << 2u)) {
          const int corr = static_cast<int>(cur[2]) - static_cast<int>(clamp(last[2], diff_l));
          m_diff[2].encode_symbol(out_stream, static_cast<uint8_t>(corr));
        }
        if (sym & (1u << 4u)) {
          diff_l = (diff_l + (static_cast<int>(cur[2]) - static_cast<int>(last[2]))) / 2;
          const int corr = static_cast<int>(cur[4]) - static_cast<int>(clamp(last[4], diff_l));
          m_diff[4].encode_symbol(out_stream, static_cast<uint8_t>(corr));
        }
        if (sym & (1u << 3u)) {
          const int corr = static_cast<int>(cur[3]) - static_cast<int>(clamp(last[3], diff_h));
          m_diff[3].encode_symbol(out_stream, static_cast<uint8_t>(corr));
        }
        if (sym & (1u << 5u)) {
          diff_h = (diff_h + (static_cast<int>(cur[3]) - static_cast<int>(last[3]))) / 2;
          const int corr = static_cast<int>(cur[5]) - static_cast<int>(clamp(last[5], diff_h));
          m_diff[5].encode_symbol(out_stream, static_cast<uint8_t>(corr));
        }
      }
    } else {
      // v1: independent bytes, residual = (cur - last)
      if (sym & (1u << 2u))
        m_diff[2].encode_symbol(out_stream, static_cast<uint8_t>(cur[2] - last[2]));
      if (sym & (1u << 3u))
        m_diff[3].encode_symbol(out_stream, static_cast<uint8_t>(cur[3] - last[3]));
      if (sym & (1u << 4u))
        m_diff[4].encode_symbol(out_stream, static_cast<uint8_t>(cur[4] - last[4]));
      if (sym & (1u << 5u))
        m_diff[5].encode_symbol(out_stream, static_cast<uint8_t>(cur[5] - last[5]));
    }

    m_last_value = color_data;
  }
};

using RGB12EncoderV1 = RGB12EncoderT<RGB12Version::V1>;
using RGB12EncoderV2 = RGB12EncoderT<RGB12Version::V2>;

// Back-compat: default RGB12 encoder is v2.
using RGB12Encoder = RGB12EncoderV2;

}  // namespace laspp
