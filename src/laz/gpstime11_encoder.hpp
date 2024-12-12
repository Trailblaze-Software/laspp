#pragma once

#include <array>
#include <cstdint>

#include "las_point.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/assert.hpp"
namespace laspp {

class GPSTime11Encoder {
  struct ReferenceFrame {
    int64_t delta;
    uint64_t counter;
    GPSTime prev_gps_time;

    ReferenceFrame() : delta(0), counter(0), prev_gps_time(0) {}

    friend std::ostream& operator<<(std::ostream& os, const ReferenceFrame& rf) {
      return os << "Delta: " << rf.delta << " Counter: " << rf.counter
                << " Prev GPS Time: " << rf.prev_gps_time;
    }
  };

  std::array<ReferenceFrame, 4> m_reference_frames;
  uint8_t m_current_frame;
  MultiInstanceIntegerEncoder<32, 9> m_dgps_time_low_encoder;
  SymbolEncoder<516> m_case_encoder;
  SymbolEncoder<6> m_case_0delta_encoder;

 public:
  using EncodedType = GPSTime;
  const EncodedType& last_value() const {
    return m_reference_frames[m_current_frame].prev_gps_time;
  }

  explicit GPSTime11Encoder(GPSTime last_gps_time) : m_current_frame(0) {
    m_reference_frames[0].prev_gps_time = last_gps_time;
  }

  GPSTime decode(InStream& in_stream) {
    uint16_t case_delta;
    if (m_reference_frames[m_current_frame].delta == 0) {
      case_delta = m_case_0delta_encoder.decode_symbol(in_stream);
      if (case_delta >= 3) {
        m_current_frame = (m_current_frame + case_delta - 2) % 4;
        if (m_reference_frames[m_current_frame].delta == 0) {
          case_delta = m_case_0delta_encoder.decode_symbol(in_stream);
          AssertLE(case_delta, 2);
        } else {
          case_delta = m_case_encoder.decode_symbol(in_stream);
          AssertLE(case_delta, 512);
        }
      }
      if (case_delta == 0) {
        case_delta = 511;
      } else if (case_delta == 2) {
        case_delta = 512;
      }
    } else {
      case_delta = m_case_encoder.decode_symbol(in_stream);
      if (case_delta >= 513) {
        m_current_frame = (m_current_frame + case_delta - 512) % 4;
        if (m_reference_frames[m_current_frame].delta == 0) {
          case_delta = m_case_0delta_encoder.decode_symbol(in_stream);
          AssertLE(case_delta, 2);
        } else {
          case_delta = m_case_encoder.decode_symbol(in_stream);
          AssertLE(case_delta, 512);
        }
      }
    }
    if (case_delta <= 510) {
      // int32_t dgps_time_low;
      if (case_delta == 0) {
        int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(7, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_uint64() += dgps_time_low;
        m_reference_frames[m_current_frame].counter++;
        if (m_reference_frames[m_current_frame].counter > 3) {
          m_reference_frames[m_current_frame].delta = dgps_time_low;
          m_reference_frames[m_current_frame].counter = 0;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta == 1) {
        int instance = m_reference_frames[m_current_frame].delta == 0 ? 0 : 1;
        int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(instance, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_uint64() +=
            m_reference_frames[m_current_frame].delta + dgps_time_low;
        m_reference_frames[m_current_frame].counter = 0;
        if (m_reference_frames[m_current_frame].delta == 0) {
          m_reference_frames[m_current_frame].delta = dgps_time_low;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta < 500) {
        int32_t dgps_time_low =
            m_dgps_time_low_encoder.decode_int(case_delta < 10 ? 2 : 3, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_uint64() +=
            case_delta * m_reference_frames[m_current_frame].delta + dgps_time_low;
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta == 500) {
        int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(4, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_uint64() +=
            case_delta * m_reference_frames[m_current_frame].delta + dgps_time_low;
        m_reference_frames[m_current_frame].counter++;
        if (m_reference_frames[m_current_frame].counter > 3) {
          m_reference_frames[m_current_frame].delta = 500 * case_delta + dgps_time_low;
          m_reference_frames[m_current_frame].counter = 0;
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      } else if (case_delta <= 510) {
        int32_t dgps_time_low =
            m_dgps_time_low_encoder.decode_int(case_delta == 510 ? 6 : 5, in_stream);
        m_reference_frames[m_current_frame].prev_gps_time.as_uint64() +=
            -(case_delta - 500) * m_reference_frames[m_current_frame].delta + dgps_time_low;
        if (case_delta == 510) {
          m_reference_frames[m_current_frame].counter++;
          if (m_reference_frames[m_current_frame].counter > 3) {
            m_reference_frames[m_current_frame].delta =
                -(case_delta - 500) * m_reference_frames[m_current_frame].delta + dgps_time_low;
            m_reference_frames[m_current_frame].counter = 0;
          }
        }
        return m_reference_frames[m_current_frame].prev_gps_time;
      }
    } else if (case_delta == 511) {
      return m_reference_frames[m_current_frame].prev_gps_time;
    } else if (case_delta == 512) {
      int32_t dgps_time_low = m_dgps_time_low_encoder.decode_int(8, in_stream);
      uint32_t dgps_time = raw_decode(in_stream, 32);
      uint64_t tmp =
          ((uint64_t)((int32_t)(m_reference_frames[m_current_frame].prev_gps_time.as_uint64() >>
                                32) +
                      dgps_time_low)
           << 32) +
          dgps_time;
      m_current_frame = (m_current_frame + 1) % 4;
      m_reference_frames[m_current_frame].prev_gps_time.as_uint64() = tmp;
      m_reference_frames[m_current_frame].delta = 0;
      m_reference_frames[m_current_frame].counter = 0;
      return m_reference_frames[m_current_frame].prev_gps_time;
    }

    Fail("Unkown case delta: ", case_delta);
  }
};

}  // namespace laspp
