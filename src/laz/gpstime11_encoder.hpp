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
#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/assert.hpp"
namespace laspp {

template <bool Point14 = false>
class GeneralGPSTimeEncoder {
  struct ReferenceFrame {
    int32_t delta;
    int32_t counter;
    GPSTime prev_gps_time;

    ReferenceFrame() : delta(0), counter(0), prev_gps_time(0) {}

    friend std::ostream& operator<<(std::ostream& os, const ReferenceFrame& rf) {
      return os << "Delta: " << rf.delta << " Counter: " << rf.counter
                << " Prev GPS Time: " << rf.prev_gps_time;
    }
  };

  std::array<ReferenceFrame, 4> m_reference_frames;
  MultiInstanceIntegerEncoder<32, 9> m_dgps_time_low_encoder;
  SymbolEncoder<516 - Point14> m_case_encoder;
  SymbolEncoder<6 - Point14> m_case_0delta_encoder;
  uint_fast8_t m_current_frame;
  uint_fast8_t m_next_unused_frame;

 public:
  using EncodedType = GPSTime;
  const EncodedType& last_value() const {
    return m_reference_frames[m_current_frame].prev_gps_time;
  }

  static constexpr bool Point14Mode = Point14;

  explicit GeneralGPSTimeEncoder(GPSTime last_gps_time)
      : m_current_frame(0), m_next_unused_frame(0) {
    m_reference_frames[0].prev_gps_time = last_gps_time;
  }

  int32_t case_delta_to_instance(int32_t case_delta) {
    if (case_delta == 0) {
      return 7;
    } else if (case_delta == 1) {
      return 1;
    } else if (case_delta < 10) {
      return 2;
    } else if (case_delta < 500) {
      return 3;
    } else if (case_delta == 500) {
      return 4;
    } else if (case_delta < 510) {
      return 5;
    } else if (case_delta == 510) {
      return 6;
    } else if (case_delta == 512 - Point14) {
      return 8;
    }
    LASPP_FAIL("Unknown case delta: ", case_delta);
  }

  GPSTime decode(InStream& in_stream) {
    uint_fast16_t case_delta;
    if (m_reference_frames[m_current_frame].delta == 0) {
      case_delta = m_case_0delta_encoder.decode_symbol(in_stream) + Point14;
      if (case_delta >= 3) {
        m_current_frame = (m_current_frame + case_delta - 2) % 4;
        return decode(in_stream);
      }
      if (!Point14 && case_delta == 0) {
        case_delta = 511;
      } else if (case_delta == 2) {
        case_delta = 512 - Point14;
      }
    } else {
      case_delta = m_case_encoder.decode_symbol(in_stream);
      if (case_delta >= 513 - Point14) {
        m_current_frame = (m_current_frame + case_delta - (512 - Point14)) % 4;
        return decode(in_stream);
      }
    }
    if (case_delta <= 510) {
      // int32_t dgps_time_low;
      if (case_delta == 0) {
        int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(7, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_int64() += dgps_time_low;
        m_reference_frames[m_current_frame].counter++;
        if (m_reference_frames[m_current_frame].counter > 3) {
          m_reference_frames[m_current_frame].delta = dgps_time_low;
          m_reference_frames[m_current_frame].counter = 0;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta == 1) {
        uint32_t instance = m_reference_frames[m_current_frame].delta == 0 ? 0u : 1u;
        int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(instance, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_int64() +=
            m_reference_frames[m_current_frame].delta + dgps_time_low;
        m_reference_frames[m_current_frame].counter = 0;
        if (m_reference_frames[m_current_frame].delta == 0) {
          m_reference_frames[m_current_frame].delta = dgps_time_low;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta < 500) {
        uint32_t dgps_time_low = static_cast<uint32_t>(
            m_dgps_time_low_encoder.decode_int(case_delta < 10 ? 2 : 3, in_stream));
        m_reference_frames[m_current_frame].prev_gps_time.as_int64() += static_cast<int32_t>(
            static_cast<uint32_t>(case_delta) *
                static_cast<uint32_t>(m_reference_frames[m_current_frame].delta) +
            dgps_time_low);
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta == 500) {
        uint32_t dgps_time_low =
            static_cast<uint32_t>(m_dgps_time_low_encoder.decode_int(4, in_stream));
        m_reference_frames[m_current_frame].prev_gps_time.as_int64() += static_cast<int32_t>(
            static_cast<uint32_t>(case_delta) *
                static_cast<uint32_t>(m_reference_frames[m_current_frame].delta) +
            dgps_time_low);
        m_reference_frames[m_current_frame].counter++;
        if (m_reference_frames[m_current_frame].counter > 3) {
          m_reference_frames[m_current_frame].delta = static_cast<int32_t>(
              static_cast<uint32_t>(500 * m_reference_frames[m_current_frame].delta) +
              dgps_time_low);
          m_reference_frames[m_current_frame].counter = 0;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta <= 510) {
        uint32_t dgps_time_low = static_cast<uint32_t>(
            m_dgps_time_low_encoder.decode_int(case_delta == 510 ? 6 : 5, in_stream));
        m_reference_frames[m_current_frame].prev_gps_time.as_int64() += static_cast<int32_t>(
            -static_cast<uint32_t>(case_delta - 500) *
                static_cast<uint32_t>(m_reference_frames[m_current_frame].delta) +
            dgps_time_low);
        if (case_delta == 510) {
          m_reference_frames[m_current_frame].counter++;
          if (m_reference_frames[m_current_frame].counter > 3) {
            m_reference_frames[m_current_frame].delta = static_cast<int32_t>(
                static_cast<uint32_t>(-10 * m_reference_frames[m_current_frame].delta) +
                dgps_time_low);
            m_reference_frames[m_current_frame].counter = 0;
          }
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      }
    } else if (!Point14 && case_delta == 511) {
      return m_reference_frames[m_current_frame].prev_gps_time;
    }
    LASPP_ASSERT_EQ(case_delta, 512 - Point14, "The final one");
    uint32_t dgps_time_low =
        static_cast<uint32_t>(m_dgps_time_low_encoder.decode_int(8, in_stream));
    uint32_t dgps_time = static_cast<uint32_t>(raw_decode(in_stream, 32));
    uint64_t tmp = (static_cast<uint64_t>(
                        static_cast<uint32_t>(
                            m_reference_frames[m_current_frame].prev_gps_time.as_uint64() >> 32) +
                        dgps_time_low)
                    << 32) +
                   dgps_time;
    m_next_unused_frame = (m_next_unused_frame + 1u) % 4;
    m_current_frame = m_next_unused_frame;
    m_reference_frames[m_current_frame].prev_gps_time.as_uint64() = tmp;
    m_reference_frames[m_current_frame].delta = 0;
    m_reference_frames[m_current_frame].counter = 0;
    return m_reference_frames[m_current_frame].prev_gps_time;
  }

  void encode(OutStream& out_stream, GPSTime gps_time) {
    ReferenceFrame& rf = m_reference_frames[m_current_frame];
    if (rf.delta == 0) {
      if (!Point14 && rf.prev_gps_time.as_uint64() == gps_time.as_uint64()) {
        m_case_0delta_encoder.encode_symbol(out_stream, 0);
        return;
      }
      int64_t diff = static_cast<int64_t>(gps_time.as_uint64() - rf.prev_gps_time.as_uint64());
      if (diff == static_cast<int32_t>(diff)) {
        int32_t diff_32 = static_cast<int32_t>(diff);
        m_case_0delta_encoder.encode_symbol(out_stream, 1 - Point14);
        m_dgps_time_low_encoder.encode_int(0, out_stream, diff_32);
        rf.delta = diff_32;
        rf.counter = 0;
      } else {
        for (size_t i = 0; i < 4; i++) {
          int64_t rf_diff = static_cast<int64_t>(gps_time.as_uint64() -
                                                 m_reference_frames[i].prev_gps_time.as_uint64());
          if (static_cast<int32_t>(rf_diff) == rf_diff) {
            m_case_0delta_encoder.encode_symbol(out_stream,
                                                2 - Point14 + (4 + i - m_current_frame) % 4);
            m_current_frame = static_cast<uint_fast8_t>(i);
            return encode(out_stream, gps_time);
          }
        }

        m_case_0delta_encoder.encode_symbol(out_stream, 2 - Point14);
        m_dgps_time_low_encoder.encode_int(
            8, out_stream,
            static_cast<int32_t>(static_cast<uint32_t>(gps_time.as_uint64() >> 32) -
                                 static_cast<uint32_t>(rf.prev_gps_time.as_uint64() >> 32)));
        raw_encode(out_stream, static_cast<uint32_t>(gps_time.as_uint64()), 32);
        m_next_unused_frame = (m_next_unused_frame + 1u) % 4;
        m_current_frame = m_next_unused_frame;
        m_reference_frames[m_current_frame].delta = 0;
        m_reference_frames[m_current_frame].counter = 0;
      }
      m_reference_frames[m_current_frame].prev_gps_time = gps_time;
    } else {
      if (!Point14 && rf.prev_gps_time.as_uint64() == gps_time.as_uint64()) {
        m_case_encoder.encode_symbol(out_stream, 511);
        return;
      }
      int64_t diff = gps_time.as_int64() - rf.prev_gps_time.as_int64();
      if (diff == static_cast<int32_t>(diff)) {
        uint32_t diff_32 = static_cast<uint32_t>(diff);
        int32_t multiplier =
            static_cast<int32_t>(diff_32 + static_cast<uint32_t>(rf.delta) / 2) / rf.delta;
        if (multiplier == 1) {
          m_case_encoder.encode_symbol(out_stream, static_cast<uint_fast16_t>(multiplier));
          m_dgps_time_low_encoder.encode_int(
              1, out_stream, static_cast<int32_t>(diff_32 - static_cast<uint32_t>(rf.delta)));
          rf.counter = 0;
        } else if (multiplier == 0) {
          m_case_encoder.encode_symbol(out_stream, static_cast<uint_fast16_t>(multiplier));
          m_dgps_time_low_encoder.encode_int(7, out_stream, static_cast<int32_t>(diff_32));
          rf.counter++;
          if (rf.counter > 3) {
            rf.delta = static_cast<int32_t>(diff_32);
            rf.counter = 0;
          }
        } else if (multiplier > 0) {
          if (multiplier < 500) {
            m_case_encoder.encode_symbol(out_stream, static_cast<uint_fast16_t>(multiplier));
            m_dgps_time_low_encoder.encode_int(
                multiplier < 10 ? 2 : 3, out_stream,
                static_cast<int32_t>(diff_32 - static_cast<uint32_t>(rf.delta) *
                                                   static_cast<uint32_t>(multiplier)));
          } else {
            multiplier = 500;
            m_case_encoder.encode_symbol(out_stream, static_cast<uint_fast16_t>(multiplier));
            m_dgps_time_low_encoder.encode_int(
                4, out_stream,
                static_cast<int32_t>(diff_32 - static_cast<uint32_t>(rf.delta) *
                                                   static_cast<uint32_t>(multiplier)));
            rf.counter++;
            if (rf.counter > 3) {
              rf.delta = static_cast<int32_t>(diff_32);
              rf.counter = 0;
            }
          }
        } else {
          if (multiplier < -10) {
            multiplier = -10;
          }
          m_case_encoder.encode_symbol(out_stream, static_cast<uint_fast16_t>(500 - multiplier));
          m_dgps_time_low_encoder.encode_int(
              multiplier == -10 ? 6 : 5, out_stream,
              static_cast<int32_t>(diff_32 - static_cast<uint32_t>(rf.delta) *
                                                 static_cast<uint32_t>(multiplier)));
          if (multiplier == -10) {
            rf.counter++;
            if (rf.counter > 3) {
              rf.delta = static_cast<int32_t>(diff_32);
              rf.counter = 0;
            }
          }
        }
        rf.prev_gps_time = gps_time;
      } else {
        for (size_t i = 0; i < 4; i++) {
          int64_t rf_diff = gps_time.as_int64() - m_reference_frames[i].prev_gps_time.as_int64();
          if (rf_diff == static_cast<int32_t>(rf_diff)) {
            m_case_encoder.encode_symbol(out_stream, 512 - Point14 + (4 + i - m_current_frame) % 4);
            m_current_frame = static_cast<uint_fast8_t>(i);
            return encode(out_stream, gps_time);
          }
        }

        m_case_encoder.encode_symbol(out_stream, 512 - Point14);
        m_dgps_time_low_encoder.encode_int(
            8, out_stream,
            static_cast<int32_t>(gps_time.as_uint64() >> 32) -
                static_cast<int32_t>(rf.prev_gps_time.as_uint64() >> 32));
        raw_encode(out_stream, static_cast<uint32_t>(gps_time.as_uint64()), 32);
        m_next_unused_frame = (m_next_unused_frame + 1u) % 4;
        m_current_frame = m_next_unused_frame;
        m_reference_frames[m_current_frame].delta = 0;
        m_reference_frames[m_current_frame].counter = 0;
        m_reference_frames[m_current_frame].prev_gps_time = gps_time;
      }
    }
  }
};

using GPSTime11Encoder = GeneralGPSTimeEncoder<>;

}  // namespace laspp
