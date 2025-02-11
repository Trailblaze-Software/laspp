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

#include <cstdint>

#include "las_point.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/streaming_median.hpp"

namespace laspp {

inline int32_t wrapping_int32_add(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

inline int32_t wrapping_int32_sub(int32_t a, int32_t b) { return wrapping_int32_add(a, -b); }

template <size_t N, typename T>
constexpr std::array<T, N> create_array(const T& val) {
  std::array<T, N> arr;
  arr.fill(val);
  return arr;
}

struct LASPointFormat6Context : LASPointFormat6 {
  bool initialized;

  std::array<SymbolEncoder<128>, 8> changed_values_encoders;
  SymbolEncoder<3> d_scanner_channel_encoder;
  std::array<SymbolEncoder<16>, 16> n_returns_encoders;
  SymbolEncoder<13> d_return_number_same_time_encoder;
  std::array<SymbolEncoder<16>, 16> return_number_diff_time_encoder;
  MultiInstanceIntegerEncoder<32, 2> dx_encoders;
  std::array<StreamingMedian<int32_t>, 16> dx_streamed_median;
  MultiInstanceIntegerEncoder<32, 22> dy_encoder;
  std::array<StreamingMedian<int32_t>, 16> dy_streamed_median;

  uint_fast8_t m;
  uint_fast8_t l;

  uint_fast8_t cpr;
  uint_fast8_t cprgps;

  LASPointFormat6Context() : initialized(false) {}

  void set_cpr(bool gps_changed = false) {
    if (return_number == 1) {
      if (number_of_returns < 2) {
        cpr = 3;
      } else {
        cpr = 2;
      }
    } else {
      if (return_number >= number_of_returns) {
        cpr = 1;
      } else {
        cpr = 0;
      }
    }
    cprgps = static_cast<uint_fast8_t>(cpr * 2 + gps_changed);
  }

  void initialize(const LASPointFormat6& las_point) {
    static_cast<LASPointFormat6&>(*this) = las_point;
    set_cpr();
    initialized = true;
  }
};

class LASPointFormat6Encoder {
  std::array<LASPointFormat6Context, 4> m_contexts;
  uint_fast8_t m_context;

  static constexpr uint8_t number_return_map_6ctx[16][16] = {
      {0, 1, 2, 3, 4, 5, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5},
      {1, 0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
      {2, 1, 2, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3},
      {3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
      {4, 3, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
      {5, 3, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
      {3, 3, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4},
      {4, 3, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4},
      {4, 3, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4},
      {5, 3, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4},
      {5, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4},
      {5, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 4, 4, 4},
      {5, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 4, 4},
      {5, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 4},
      {5, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5},
      {5, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5}};

  static constexpr uint8_t number_return_level_8ctx[16][16] = {
      {0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7},
      {1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7},
      {2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7},
      {3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7},
      {4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7},
      {5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7},
      {6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7},
      {7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7},
      {7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7},
      {7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6},
      {7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5},
      {7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4},
      {7, 7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3},
      {7, 7, 7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2},
      {7, 7, 7, 7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 1},
      {7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0}};

 public:
  using EncodedType = LASPointFormat6;
  const LASPointFormat6& last_value() const { return m_contexts[m_context]; }

  explicit LASPointFormat6Encoder(const LASPointFormat6& initial_las_point) {
    m_context = initial_las_point.scanner_channel;
    m_contexts[m_context].initialize(initial_las_point);
  }

  LASPointFormat6 decode(InStream& stream) {
    uint_fast16_t changed_values =
        m_contexts[m_context].changed_values_encoders[m_contexts[m_context].cprgps].decode_symbol(
            stream);
    if (changed_values & (1 << 6)) {
      uint_fast16_t d_scanner_channel =
          1 + m_contexts[m_context].d_scanner_channel_encoder.decode_symbol(stream);
      uint_fast16_t new_scanner_channel =
          (m_contexts[m_context].scanner_channel + d_scanner_channel) % 4;
      if (!m_contexts[new_scanner_channel].initialized) {
        m_contexts[new_scanner_channel].initialize(m_contexts[m_context]);
      }
      m_context = static_cast<uint_fast8_t>(new_scanner_channel);
    }
    LASPointFormat6Context& context = m_contexts[m_context];

    if (changed_values) {
      if (changed_values & (1 << 2)) {
        context.number_of_returns =
            static_cast<uint8_t>(
                context.n_returns_encoders[context.number_of_returns].decode_symbol(stream)) &
            0xf;
      }

      if (changed_values & (0b11)) {
        uint_fast16_t d_return_number;
        if (changed_values & 0b1 && changed_values & 0b10) {
          d_return_number = 2 + context.d_return_number_same_time_encoder.decode_symbol(stream);
        } else if (changed_values & 0b1) {
          d_return_number = 1;
        } else {
          d_return_number = 15;
        }
        context.return_number = (context.return_number + d_return_number) % 16;
      }

      context.m = number_return_map_6ctx[context.number_of_returns][context.return_number];
      context.l = number_return_level_8ctx[context.number_of_returns][context.return_number];
    }

    uint32_t mgps = context.m * 2u + static_cast<uint32_t>(bool(changed_values & (1 << 4)));
    int32_t decoded_int = context.dx_encoders[context.number_of_returns == 1].decode_int(stream);

    int32_t dx = wrapping_int32_add(decoded_int, context.dx_streamed_median[mgps].get_median());
    context.dx_streamed_median[mgps].insert(dx);
    context.x = wrapping_int32_add(context.x, dx);

    uint_fast16_t dx_k = context.dx_encoders[mgps].prev_k();

    // Y
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (context.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy = wrapping_int32_add(context.dy_encoder[dy_instance].decode_int(stream),
                                    context.dy_streamed_median[context.m].get_median());
    context.y = wrapping_int32_add(context.y, dy);
    context.dy_streamed_median[context.m].insert(dy);

    return context;
  }

  void encode(OutStream& stream, const LASPointFormat6& point) {
    uint_fast16_t d_scanner_channel =
        ((point.scanner_channel - m_contexts[m_context].scanner_channel) + 4u) % 4u;

    uint_fast8_t old_cprgps = m_contexts[m_context].cprgps;
    uint_fast16_t changed_values = 0;
    if (d_scanner_channel) {
      changed_values |= (1 << 6);
      if (!m_contexts[point.scanner_channel].initialized) {
        m_contexts[point.scanner_channel].initialize(m_contexts[m_context]);
      }
      m_context = static_cast<uint_fast8_t>(point.scanner_channel);
    }
    LASPointFormat6Context& context = m_contexts[m_context];
    if (point.point_source_id != context.point_source_id) {
      changed_values |= (1 << 5);
    }
    if (point.gps_time != context.gps_time) {
      changed_values |= (1 << 4);
    }
    if (point.scan_angle != context.scan_angle) {
      changed_values |= (1 << 3);
    }
    if (point.number_of_returns != context.number_of_returns) {
      changed_values |= (1 << 2);
    }
    if (point.return_number != context.return_number) {
      uint_fast16_t d_return_number = (point.return_number - context.return_number + 16u) % 16u;
      if (d_return_number == 1) {
        changed_values |= 0b1;
      } else if (d_return_number == 15) {
        changed_values |= 0b10;
      } else {
        changed_values |= 0b11;
      }
    }
    context.changed_values_encoders[old_cprgps].encode_symbol(stream, changed_values);

    if (changed_values) {
      if (changed_values & (1 << 6)) {
        context.d_scanner_channel_encoder.encode_symbol(stream, d_scanner_channel - 1);
      }
      if (changed_values & (1 << 2)) {
        context.n_returns_encoders[context.number_of_returns].encode_symbol(
            stream, point.number_of_returns);
        context.number_of_returns = point.number_of_returns;
      }
      if (changed_values & 0b1 && changed_values & 0b10) {
        uint_fast16_t d_return_number = (point.return_number - context.return_number + 16u) % 16u;
        context.d_return_number_same_time_encoder.encode_symbol(stream, d_return_number - 2);
        context.return_number = point.return_number;
      }

      context.m = number_return_map_6ctx[context.number_of_returns][context.return_number];
      context.l = number_return_level_8ctx[context.number_of_returns][context.return_number];
    }

    uint32_t mgps = context.m * 2u + static_cast<uint32_t>(bool(changed_values & (1 << 4)));

    // X
    int32_t dx = wrapping_int32_sub(point.x, context.x);
    context.dx_encoders[context.number_of_returns == 1].encode_int(
        stream, wrapping_int32_sub(dx, context.dx_streamed_median[mgps].get_median()));
    context.dx_streamed_median[mgps].insert(dx);
    context.x = point.x;

    uint_fast16_t dx_k = context.dx_encoders[context.number_of_returns == 1].prev_k();

    // Y
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (context.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy = wrapping_int32_sub(point.y, context.y);
    context.dy_encoder[dy_instance].encode_int(
        stream, wrapping_int32_sub(dy, context.dy_streamed_median[mgps].get_median()));
    context.y = point.y;
    context.dy_streamed_median[mgps].insert(dy);
  }
};

}  // namespace laspp
