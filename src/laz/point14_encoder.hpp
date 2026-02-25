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

#include <array>
#include <cstdint>

#include "las_point.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/layered_stream.hpp"
#include "laz/streaming_median.hpp"
#include "utilities/arithmetic.hpp"

namespace laspp {

#define LASPP_CHANNEL_RETURNS_LAYER 0
#define LASPP_Z_LAYER 1
#define LASPP_CLASSIFICATION_LAYER 2
#define LASPP_FLAGS_LAYER 3
#define LASPP_INTENSITY_LAYER 4
#define LASPP_SCAN_ANGLE_LAYER 5
#define LASPP_USER_DATA_LAYER 6
#define LASPP_POINT_SOURCE_LAYER 7
#define LASPP_GPS_TIME_LAYER 8

#define LASPP_POINT14_RETURN_NUMBER_CHANGED_CONTEXT_BITS 0x3
#define LASPP_POINT14_NUMBER_OF_RETURNS_CHANGED_CONTEXT_BIT 1 << 2
#define LASPP_POINT14_SCAN_ANGLE_CHANGED_CONTEXT_BIT 1 << 3
#define LASPP_POINT14_GPS_TIME_CHANGED_CONTEXT_BIT 1 << 4
#define LASPP_POINT14_POINT_SOURCE_CHANGED_CONTEXT_BIT 1 << 5
#define LASPP_POINT14_SCANNER_CHANNEL_CHANGED_CONTEXT_BIT 1 << 6

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
  MultiInstanceIntegerEncoder<32, 22> dz_encoder;
  std::array<int32_t, 16> last_z;
  std::array<SymbolEncoder<256>, 64> classification_encoders;
  std::array<SymbolEncoder<64>, 64> flag_encoders;
  std::array<SymbolEncoder<256>, 64> user_data_encoders;
  MultiInstanceIntegerEncoder<16, 8> intensity_encoder;
  std::array<uint16_t, 8> last_intensity;
  MultiInstanceIntegerEncoder<16, 2> scan_angle_encoder;
  IntegerEncoder<16> point_source_id_encoder;
  GeneralGPSTimeEncoder<true> gps_time_encoder;

  uint_fast8_t m;
  uint_fast8_t l;

  uint_fast8_t cpr;
  uint_fast8_t cprgps;

  static constexpr int NUM_LAYERS = 9;
  using LayerInStreams = LayeredInStreams<NUM_LAYERS>;
  using LayerOutStreams = LayeredOutStreams<NUM_LAYERS>;

  LASPointFormat6Context()
      : initialized(false), gps_time_encoder(GPSTime(0)), m(0), l(0), cpr(0), cprgps(0) {}

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

  inline void set_cpr(bool gps_changed = false) {
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

  inline void set_m_l() {
    m = number_return_map_6ctx[number_of_returns][return_number];
    l = number_return_level_8ctx[number_of_returns][return_number];
  }

  inline void initialize(const LASPointFormat6& las_point) {
    static_cast<LASPointFormat6&>(*this) = las_point;
    set_cpr();
    set_m_l();
    // Copy values to local variables to avoid misaligned reference issues
    // with packed structures when passing to std::array::fill()
    const int32_t z_local = las_point.z;
    const uint16_t intensity_local = las_point.intensity;
    last_z.fill(z_local);
    last_intensity.fill(intensity_local);
    gps_time_encoder.set_previous_value(GPSTime(las_point.gps_time));
    initialized = true;
  }

  inline void decode_return_number(uint_fast16_t return_flag, bool gps_time_change,
                                   LayerInStreams& streams) {
    uint8_t last_return_number = return_number;
    if (return_flag == 0b01) {
      return_number = static_cast<uint8_t>((last_return_number + 1) & 0xF);
    } else if (return_flag == 0b10) {
      return_number = static_cast<uint8_t>((last_return_number + 15) & 0xF);
    } else if (return_flag == 0b11) {
      if (gps_time_change) {
        uint_fast16_t decoded_return =
            return_number_diff_time_encoder[last_return_number].decode_symbol(
                streams[LASPP_CHANNEL_RETURNS_LAYER]);
        return_number = static_cast<uint8_t>(decoded_return & 0xF);
      } else {
        uint_fast16_t sym =
            d_return_number_same_time_encoder.decode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER]);
        return_number = static_cast<uint8_t>((last_return_number + sym + 2) & 0xF);
      }
    }
  }

  inline void decode_number_of_returns(uint_fast16_t changed_values, LayerInStreams& streams) {
    if (changed_values & LASPP_POINT14_NUMBER_OF_RETURNS_CHANGED_CONTEXT_BIT) {
      uint_fast16_t new_number_of_returns =
          n_returns_encoders[number_of_returns].decode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER]);
      number_of_returns = static_cast<uint8_t>(new_number_of_returns & 0xF);
    }
  }

  inline void decode_coordinates(bool gps_time_change, LayerInStreams& streams) {
    uint32_t mgps = m * 2u + static_cast<uint32_t>(gps_time_change);
    bool single_return = number_of_returns == 1;

    int32_t decoded_dx =
        dx_encoders[single_return].decode_int(streams[LASPP_CHANNEL_RETURNS_LAYER]);
    int32_t dx = wrapping_int32_add(decoded_dx, dx_streamed_median[mgps].get_median());
    dx_streamed_median[mgps].insert(dx);
    x = wrapping_int32_add(x, dx);

    uint_fast16_t dx_k = dx_encoders[single_return].prev_k();
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (single_return) {
      dy_instance++;
    }
    int32_t decoded_dy = dy_encoder[dy_instance].decode_int(streams[LASPP_CHANNEL_RETURNS_LAYER]);
    int32_t dy = wrapping_int32_add(decoded_dy, dy_streamed_median[mgps].get_median());
    dy_streamed_median[mgps].insert(dy);
    y = wrapping_int32_add(y, dy);

    uint_fast16_t dy_k = dy_encoder[dy_instance].prev_k();
    uint_fast16_t kxy_sum = (dx_k + dy_k) / 2;
    uint32_t kxy = static_cast<uint32_t>(kxy_sum);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (single_return) {
      dz_instance++;
    }
    int32_t decoded_dz = dz_encoder[dz_instance].decode_int(streams[LASPP_Z_LAYER]);
    z = wrapping_int32_add(last_z[l], decoded_dz);
    last_z[l] = z;
  }

  inline void decode_classification(LayerInStreams& streams) {
    if (!streams.non_empty(LASPP_CLASSIFICATION_LAYER)) {
      return;
    }
    uint8_t last_classification = static_cast<uint8_t>(classification);
    uint8_t classification_idx =
        static_cast<uint8_t>(((last_classification & 0x1F) << 1) + (cpr == 3 ? 1 : 0));
    uint_fast16_t decoded_classification =
        classification_encoders[classification_idx].decode_symbol(
            streams[LASPP_CLASSIFICATION_LAYER]);
    classification = static_cast<LASClassification>(decoded_classification);
  }

  inline void decode_flags(LayerInStreams& streams) {
    if (!streams.non_empty(LASPP_FLAGS_LAYER)) {
      return;
    }
    uint8_t last_flags = static_cast<uint8_t>((edge_of_flight_line << 5) |
                                              (scan_direction_flag << 4) | classification_flags);
    uint_fast16_t decoded_flags =
        flag_encoders[last_flags].decode_symbol(streams[LASPP_FLAGS_LAYER]);
    edge_of_flight_line = static_cast<uint8_t>((decoded_flags >> 5) & 0x1);
    scan_direction_flag = static_cast<uint8_t>((decoded_flags >> 4) & 0x1);
    classification_flags = static_cast<uint8_t>(decoded_flags & 0x0F);
  }

  inline void decode_intensity(LayerInStreams& streams) {
    if (!streams.non_empty(LASPP_INTENSITY_LAYER)) {
      return;
    }
    int32_t intensity_delta = intensity_encoder[cpr].decode_int(streams[LASPP_INTENSITY_LAYER]);
    int32_t new_intensity = static_cast<int32_t>(last_intensity[cprgps]) + intensity_delta;
    intensity = static_cast<uint16_t>(new_intensity);
    last_intensity[cprgps] = intensity;
  }

  inline void decode_user_data(LayerInStreams& streams) {
    if (!streams.non_empty(LASPP_USER_DATA_LAYER)) {
      return;
    }
    uint8_t user_ctx = static_cast<uint8_t>(user_data / 4u);
    uint_fast16_t decoded_user_data =
        user_data_encoders[user_ctx].decode_symbol(streams[LASPP_USER_DATA_LAYER]);
    user_data = static_cast<uint8_t>(decoded_user_data);
  }

  inline void encode_return_number(uint_fast16_t return_flag, uint8_t last_return_number,
                                   uint_fast16_t rn_diff, bool gps_time_change,
                                   const LASPointFormat6& point, LayerOutStreams& streams) {
    uint8_t new_return_number = last_return_number;
    if (return_flag == 0b01) {
      new_return_number = static_cast<uint8_t>((last_return_number + 1) & 0xF);
    } else if (return_flag == 0b10) {
      new_return_number = static_cast<uint8_t>((last_return_number + 15) & 0xF);
    } else if (return_flag == 0b11) {
      if (gps_time_change) {
        return_number_diff_time_encoder[last_return_number].encode_symbol(
            streams[LASPP_CHANNEL_RETURNS_LAYER], point.return_number);
        new_return_number = point.return_number;
      } else {
        d_return_number_same_time_encoder.encode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER],
                                                        rn_diff - 2);
        new_return_number = static_cast<uint8_t>((last_return_number + rn_diff) & 0xF);
      }
    }
    return_number = static_cast<uint8_t>(new_return_number & 0xF);
  }

  inline void encode_number_of_returns(uint_fast16_t changed_values, uint8_t last_number_of_returns,
                                       const LASPointFormat6& point, LayerOutStreams& streams) {
    if (changed_values & LASPP_POINT14_NUMBER_OF_RETURNS_CHANGED_CONTEXT_BIT) {
      n_returns_encoders[last_number_of_returns].encode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER],
                                                               point.number_of_returns);
      number_of_returns = static_cast<uint8_t>(point.number_of_returns & 0xF);
    } else {
      number_of_returns = static_cast<uint8_t>(last_number_of_returns & 0xF);
    }
  }

  inline void encode_coordinates(bool gps_changed, const LASPointFormat6& point,
                                 LayerOutStreams& streams) {
    uint32_t mgps = m * 2u + static_cast<uint32_t>(gps_changed);
    bool single_return = number_of_returns == 1;

    int32_t dx = wrapping_int32_sub(point.x, x);
    dx_encoders[single_return].encode_int(
        streams[LASPP_CHANNEL_RETURNS_LAYER],
        wrapping_int32_sub(dx, dx_streamed_median[mgps].get_median()));
    dx_streamed_median[mgps].insert(dx);
    x = point.x;

    uint_fast16_t dx_k = dx_encoders[single_return].prev_k();
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (single_return) {
      dy_instance++;
    }
    int32_t dy = wrapping_int32_sub(point.y, y);
    dy_encoder[dy_instance].encode_int(
        streams[LASPP_CHANNEL_RETURNS_LAYER],
        wrapping_int32_sub(dy, dy_streamed_median[mgps].get_median()));
    dy_streamed_median[mgps].insert(dy);
    y = point.y;

    uint_fast16_t dy_k = dy_encoder[dy_instance].prev_k();
    uint_fast16_t kxy_sum = (dx_k + dy_k) / 2;
    uint32_t kxy = static_cast<uint32_t>(kxy_sum);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (single_return) {
      dz_instance++;
    }
    int32_t dz = wrapping_int32_sub(point.z, last_z[l]);
    dz_encoder[dz_instance].encode_int(streams[LASPP_Z_LAYER], dz);
    last_z[l] = point.z;
    z = point.z;
  }

  inline void encode_classification(const LASClassification last_classification,
                                    const LASPointFormat6& point, LayerOutStreams& streams) {
    uint8_t classification_idx = static_cast<uint8_t>(
        ((static_cast<uint8_t>(last_classification) & 0x1F) << 1) + (cpr == 3 ? 1 : 0));
    classification_encoders[classification_idx].encode_symbol(
        streams[LASPP_CLASSIFICATION_LAYER], static_cast<uint8_t>(point.classification));
    classification = point.classification;
  }

  inline void encode_flags(uint8_t last_flags, const LASPointFormat6& point,
                           LayerOutStreams& streams) {
    flag_encoders[last_flags].encode_symbol(
        streams[LASPP_FLAGS_LAYER],
        static_cast<uint8_t>((point.edge_of_flight_line << 5) | (point.scan_direction_flag << 4) |
                             point.classification_flags));
    edge_of_flight_line = point.edge_of_flight_line;
    scan_direction_flag = point.scan_direction_flag;
    classification_flags = point.classification_flags;
  }

  inline void encode_intensity(const LASPointFormat6& point, LayerOutStreams& streams) {
    int32_t intensity_base = static_cast<int32_t>(last_intensity[cprgps]);
    int32_t intensity_diff = static_cast<int32_t>(point.intensity) - intensity_base;
    intensity_encoder[cpr].encode_int(streams[LASPP_INTENSITY_LAYER], intensity_diff);
    last_intensity[cprgps] = point.intensity;
    intensity = point.intensity;
  }

  inline void encode_user_data(uint8_t prev_user_data, const LASPointFormat6& point,
                               LayerOutStreams& streams) {
    uint8_t user_ctx = static_cast<uint8_t>(prev_user_data / 4u);
    user_data_encoders[user_ctx].encode_symbol(streams[LASPP_USER_DATA_LAYER], point.user_data);
    user_data = point.user_data;
  }
};

template <int Version = 3>
class LASPointFormat6EncoderBase {
  std::array<LASPointFormat6Context, 4> m_contexts;
  uint_fast8_t m_context;
  uint_fast8_t m_external_context;

 public:
  static constexpr int NUM_LAYERS = LASPointFormat6Context::NUM_LAYERS;
  using LayerInStreams = LayeredInStreams<NUM_LAYERS>;
  using LayerOutStreams = LayeredOutStreams<NUM_LAYERS>;

  using EncodedType = LASPointFormat6;
  const LASPointFormat6& last_value() const { return m_contexts[m_context]; }

  explicit LASPointFormat6EncoderBase(const LASPointFormat6& initial_las_point) {
    m_context = initial_las_point.scanner_channel;
    m_external_context = m_context;
    m_contexts[m_context].initialize(initial_las_point);
  }

  uint8_t get_active_context() const { return m_external_context; }

 private:
  inline void handle_scanner_channel_context_change(LASPointFormat6Context& prev_context,
                                                    uint_fast16_t changed_values,
                                                    LayerInStreams& streams) {
    if (!(changed_values & LASPP_POINT14_SCANNER_CHANNEL_CHANGED_CONTEXT_BIT)) {
      // Workaround for issue with context setting in V3: https://github.com/LASzip/LASzip/issues/74
      m_external_context = 0;
      return;
    }
    uint_fast16_t d_scanner_channel =
        prev_context.d_scanner_channel_encoder.decode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER]);
    uint_fast8_t new_scanner_channel =
        static_cast<uint_fast8_t>((m_context + d_scanner_channel + 1) % 4);
    if (!m_contexts[new_scanner_channel].initialized) {
      m_contexts[new_scanner_channel].initialize(prev_context);
      m_contexts[new_scanner_channel].scanner_channel =
          static_cast<uint8_t>(new_scanner_channel & 0x3);
    }
    m_context = new_scanner_channel;
    m_external_context = m_context;
  }

 public:
  inline LASPointFormat6 decode(LayerInStreams& streams) {
    LASPointFormat6Context& prev_context = m_contexts[m_context];
    uint_fast16_t changed_values =
        prev_context.changed_values_encoders[prev_context.cprgps].decode_symbol(
            streams[LASPP_CHANNEL_RETURNS_LAYER]);
    bool gps_time_changed =
        static_cast<bool>(changed_values & LASPP_POINT14_GPS_TIME_CHANGED_CONTEXT_BIT);

    handle_scanner_channel_context_change(prev_context, changed_values, streams);

    LASPointFormat6Context& context = m_contexts[m_context];

    context.decode_number_of_returns(changed_values, streams);

    uint_fast16_t return_flag = changed_values & LASPP_POINT14_RETURN_NUMBER_CHANGED_CONTEXT_BITS;
    context.decode_return_number(return_flag, gps_time_changed, streams);

    context.set_cpr(gps_time_changed);
    context.set_m_l();

    if (changed_values & LASPP_POINT14_POINT_SOURCE_CHANGED_CONTEXT_BIT) {
      int32_t diff = context.point_source_id_encoder.decode_int(streams[LASPP_POINT_SOURCE_LAYER]);
      context.point_source_id =
          static_cast<uint16_t>(static_cast<int32_t>(context.point_source_id) + diff);
    }

    if (gps_time_changed) {
      GPSTime new_time = context.gps_time_encoder.decode(streams[LASPP_GPS_TIME_LAYER]);
      context.gps_time = static_cast<double>(new_time);
    }

    if (changed_values & LASPP_POINT14_SCAN_ANGLE_CHANGED_CONTEXT_BIT) {
      int16_t prev_scan_angle = context.scan_angle;
      int16_t diff = static_cast<int16_t>(
          context.scan_angle_encoder[gps_time_changed].decode_int(streams[LASPP_SCAN_ANGLE_LAYER]));
      context.scan_angle = prev_scan_angle + diff;
    }

    context.decode_classification(streams);
    context.decode_coordinates(gps_time_changed, streams);
    context.decode_flags(streams);
    context.decode_intensity(streams);
    context.decode_user_data(streams);

    return context;
  }

 private:
  inline uint_fast16_t compute_changed_values(const LASPointFormat6& point, bool scanner_changed,
                                              bool point_source_change, bool gps_time_change,
                                              bool scan_angle_change,
                                              uint8_t last_number_of_returns,
                                              uint8_t last_return_number) {
    uint_fast16_t changed_values = 0;
    if (scanner_changed) {
      changed_values |= LASPP_POINT14_SCANNER_CHANNEL_CHANGED_CONTEXT_BIT;
    }
    if (point_source_change) {
      changed_values |= LASPP_POINT14_POINT_SOURCE_CHANGED_CONTEXT_BIT;
    }
    if (gps_time_change) {
      changed_values |= LASPP_POINT14_GPS_TIME_CHANGED_CONTEXT_BIT;
    }
    if (scan_angle_change) {
      changed_values |= LASPP_POINT14_SCAN_ANGLE_CHANGED_CONTEXT_BIT;
    }
    if (point.number_of_returns != last_number_of_returns) {
      changed_values |= LASPP_POINT14_NUMBER_OF_RETURNS_CHANGED_CONTEXT_BIT;
    }
    uint_fast16_t rn_diff = (point.return_number - last_return_number + 16u) % 16u;
    if (rn_diff != 0) {
      if (rn_diff == 1) {
        changed_values |= 0b1;
      } else if (rn_diff == 15) {
        changed_values |= 0b10;
      } else {
        changed_values |= 0b11;
      }
    }
    return changed_values;
  }

  inline void handle_encode_scanner_change(LASPointFormat6Context& prev_context,
                                           const LASPointFormat6& point,
                                           uint_fast8_t target_context_idx,
                                           LayerOutStreams& streams) {
    uint_fast16_t delta = (point.scanner_channel - prev_context.scanner_channel + 4u) % 4u;
    prev_context.d_scanner_channel_encoder.encode_symbol(streams[LASPP_CHANNEL_RETURNS_LAYER],
                                                         delta - 1);
    if (!m_contexts[target_context_idx].initialized) {
      m_contexts[target_context_idx].initialize(prev_context);
      m_contexts[target_context_idx].scanner_channel =
          static_cast<uint8_t>(target_context_idx & 0x3);
    }
    m_context = target_context_idx;
    m_external_context = m_context;
  }

  inline LASPointFormat6Context* get_reference_context(LASPointFormat6Context& prev_context,
                                                       uint_fast8_t target_context_idx,
                                                       bool scanner_changed) {
    if (scanner_changed && m_contexts[target_context_idx].initialized) {
      return &m_contexts[target_context_idx];
    }
    return &prev_context;
  }

  inline void encode_metadata_fields(LASPointFormat6Context& context, const LASPointFormat6& point,
                                     LASPointFormat6Context& reference_context, bool gps_changed,
                                     LayerOutStreams& streams) {
    bool point_source_change = point.point_source_id != reference_context.point_source_id;
    if (point_source_change) {
      int32_t diff = static_cast<int32_t>(point.point_source_id) -
                     static_cast<int32_t>(reference_context.point_source_id);
      context.point_source_id_encoder.encode_int(streams[LASPP_POINT_SOURCE_LAYER], diff);
    }
    context.point_source_id = point.point_source_id;

    bool gps_time_change = point.gps_time != reference_context.gps_time;
    if (gps_time_change) {
      context.gps_time_encoder.encode(streams[LASPP_GPS_TIME_LAYER], GPSTime(point.gps_time));
    }
    context.gps_time = point.gps_time;

    bool scan_angle_change = point.scan_angle != reference_context.scan_angle;
    if (scan_angle_change) {
      int16_t prev_angle = reference_context.scan_angle;
      int16_t next_angle = point.scan_angle;
      int32_t diff = static_cast<int32_t>(next_angle) - static_cast<int32_t>(prev_angle);
      context.scan_angle_encoder[gps_changed].encode_int(streams[LASPP_SCAN_ANGLE_LAYER], diff);
    }
    context.scan_angle = point.scan_angle;
  }

 public:
  inline void encode(LayerOutStreams& streams, const LASPointFormat6& point) {
    LASPointFormat6Context& prev_context = m_contexts[m_context];
    uint_fast8_t target_context_idx = point.scanner_channel;
    bool scanner_changed = target_context_idx != m_context;

    LASPointFormat6Context* reference_context =
        get_reference_context(prev_context, target_context_idx, scanner_changed);

    bool point_source_change = point.point_source_id != reference_context->point_source_id;
    bool gps_time_change = point.gps_time != reference_context->gps_time;
    bool scan_angle_change = point.scan_angle != reference_context->scan_angle;
    uint8_t last_number_of_returns = reference_context->number_of_returns;
    uint8_t last_return_number = reference_context->return_number;

    uint_fast16_t changed_values =
        compute_changed_values(point, scanner_changed, point_source_change, gps_time_change,
                               scan_angle_change, last_number_of_returns, last_return_number);

    prev_context.changed_values_encoders[prev_context.cprgps].encode_symbol(
        streams[LASPP_CHANNEL_RETURNS_LAYER], changed_values);

    if (scanner_changed) {
      handle_encode_scanner_change(prev_context, point, target_context_idx, streams);
    } else {
      // Workaround for issue with context setting in V3:
      m_external_context = static_cast<uint_fast8_t>(Version == 3 ? 0 : m_context);
    }

    LASPointFormat6Context& context = m_contexts[m_context];
    context.encode_number_of_returns(changed_values, last_number_of_returns, point, streams);

    uint_fast16_t return_flag = changed_values & LASPP_POINT14_RETURN_NUMBER_CHANGED_CONTEXT_BITS;
    uint_fast16_t rn_diff = (point.return_number - last_return_number + 16u) % 16u;
    context.encode_return_number(return_flag, last_return_number, rn_diff, gps_time_change, point,
                                 streams);

    bool gps_time_changed = gps_time_change;
    context.set_cpr(gps_time_changed);
    context.set_m_l();

    encode_metadata_fields(context, point, *reference_context, gps_time_changed, streams);
    context.encode_classification(reference_context->classification, point, streams);
    uint8_t last_flags = static_cast<uint8_t>((reference_context->edge_of_flight_line << 5) |
                                              (reference_context->scan_direction_flag << 4) |
                                              reference_context->classification_flags);
    context.encode_flags(last_flags, point, streams);
    context.encode_intensity(point, streams);
    context.encode_user_data(reference_context->user_data, point, streams);
    context.encode_coordinates(gps_time_changed, point, streams);
  }
};

using LASPointFormat6Encoder = LASPointFormat6EncoderBase<>;

}  // namespace laspp
