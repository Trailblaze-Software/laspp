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
#include "laz/gpstime11_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/layered_stream.hpp"
#include "laz/streaming_median.hpp"
#include "utilities/arithmetic.hpp"

namespace laspp {

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

  void set_m_l() {
    m = number_return_map_6ctx[number_of_returns][return_number];
    l = number_return_level_8ctx[number_of_returns][return_number];
  }

  void initialize(const LASPointFormat6& las_point) {
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
};

#define LASPP_CHANNEL_RETURNS_STREAM 0
#define LASPP_Z_STREAM 1
#define LASPP_CLASSIFICATION_STREAM 2
#define LASPP_FLAGS_STREAM 3
#define LASPP_INTENSITY_STREAM 4
#define LASPP_SCAN_ANGLE_STREAM 5
#define LASPP_USER_DATA_STREAM 6
#define LASPP_POINT_SOURCE_STREAM 7
#define LASPP_GPS_TIME_STREAM 8

template <int Version = 3>
class LASPointFormat6EncoderBase {
  std::array<LASPointFormat6Context, 4> m_contexts;
  uint_fast8_t m_context;
  uint_fast8_t m_external_context;

 public:
  static constexpr int NUM_LAYERS = 9;

  using EncodedType = LASPointFormat6;
  const LASPointFormat6& last_value() const { return m_contexts[m_context]; }

  explicit LASPointFormat6EncoderBase(const LASPointFormat6& initial_las_point) {
    m_context = initial_las_point.scanner_channel;
    m_external_context = m_context;
    m_contexts[m_context].initialize(initial_las_point);
  }

  uint8_t get_active_context() const { return m_external_context; }

  LASPointFormat6 decode(LayeredInStreams<NUM_LAYERS>& streams) {
    LASPointFormat6Context& prev_context = m_contexts[m_context];
    uint_fast16_t changed_values =
        prev_context.changed_values_encoders[prev_context.cprgps].decode_symbol(
            streams[LASPP_CHANNEL_RETURNS_STREAM]);

    bool point_source_change = static_cast<bool>(changed_values & (1 << 5));
    bool gps_time_change = static_cast<bool>(changed_values & (1 << 4));
    bool scan_angle_change = static_cast<bool>(changed_values & (1 << 3));

    if (changed_values & (1 << 6)) {
      uint_fast16_t d_scanner_channel = prev_context.d_scanner_channel_encoder.decode_symbol(
          streams[LASPP_CHANNEL_RETURNS_STREAM]);
      uint_fast8_t new_scanner_channel =
          static_cast<uint_fast8_t>((m_context + d_scanner_channel + 1) % 4);
      if (!m_contexts[new_scanner_channel].initialized) {
        // Initialize from previous context, but don't copy GPS encoder state
        // Each scanner channel needs its own independent GPS time tracking
        m_contexts[new_scanner_channel].initialize(prev_context);
        m_contexts[new_scanner_channel].scanner_channel =
            static_cast<uint8_t>(new_scanner_channel & 0x3);
      }
      m_context = new_scanner_channel;
      m_external_context = m_context;
    } else {
      // Workaround for issue with context setting in V3: https://github.com/LASzip/LASzip/issues/74
      m_external_context = 0;
    }

    LASPointFormat6Context& context = m_contexts[m_context];

    uint8_t last_number_of_returns = context.number_of_returns;
    uint8_t last_return_number = context.return_number;

    if (changed_values & (1 << 2)) {
      uint_fast16_t new_number_of_returns =
          context.n_returns_encoders[last_number_of_returns].decode_symbol(
              streams[LASPP_CHANNEL_RETURNS_STREAM]);
      context.number_of_returns = static_cast<uint8_t>(new_number_of_returns & 0xF);
    }

    uint_fast16_t return_flag = changed_values & 0b11;
    if (return_flag == 0b01) {
      context.return_number = static_cast<uint8_t>((last_return_number + 1) & 0xF);
    } else if (return_flag == 0b10) {
      context.return_number = static_cast<uint8_t>((last_return_number + 15) & 0xF);
    } else if (return_flag == 0b11) {
      if (gps_time_change) {
        uint_fast16_t decoded_return =
            context.return_number_diff_time_encoder[last_return_number].decode_symbol(
                streams[LASPP_CHANNEL_RETURNS_STREAM]);
        context.return_number = static_cast<uint8_t>(decoded_return & 0xF);
      } else {
        uint_fast16_t sym = context.d_return_number_same_time_encoder.decode_symbol(
            streams[LASPP_CHANNEL_RETURNS_STREAM]);
        context.return_number = static_cast<uint8_t>((last_return_number + sym + 2) & 0xF);
      }
    }

    context.set_cpr(gps_time_change);
    context.set_m_l();

    if (point_source_change) {
      int32_t diff = context.point_source_id_encoder.decode_int(streams[LASPP_POINT_SOURCE_STREAM]);
      context.point_source_id =
          static_cast<uint16_t>(static_cast<int32_t>(context.point_source_id) + diff);
    }

    if (gps_time_change) {
      GPSTime new_time = context.gps_time_encoder.decode(streams[LASPP_GPS_TIME_STREAM]);
      context.gps_time = static_cast<double>(new_time);
    }

    if (scan_angle_change) {
      int16_t prev_scan_angle = context.scan_angle;
      int16_t diff = static_cast<int16_t>(
          context.scan_angle_encoder[gps_time_change].decode_int(streams[LASPP_SCAN_ANGLE_STREAM]));
      int16_t updated_scan_angle = prev_scan_angle + diff;
      context.scan_angle = updated_scan_angle;
    }

    if (streams.non_empty(LASPP_CLASSIFICATION_STREAM)) {
      uint8_t last_classification = static_cast<uint8_t>(context.classification);
      uint8_t classification_idx =
          static_cast<uint8_t>(((last_classification & 0x1F) << 1) + (context.cpr == 3 ? 1 : 0));
      uint_fast16_t decoded_classification =
          context.classification_encoders[classification_idx].decode_symbol(
              streams[LASPP_CLASSIFICATION_STREAM]);
      context.classification = static_cast<LASClassification>(decoded_classification);
    }

    uint32_t mgps = context.m * 2u + static_cast<uint32_t>(gps_time_change);
    int32_t decoded_dx = context.dx_encoders[context.number_of_returns == 1].decode_int(
        streams[LASPP_CHANNEL_RETURNS_STREAM]);
    int32_t dx = wrapping_int32_add(decoded_dx, context.dx_streamed_median[mgps].get_median());
    context.dx_streamed_median[mgps].insert(dx);
    context.x = wrapping_int32_add(context.x, dx);

    uint_fast16_t dx_k = context.dx_encoders[context.number_of_returns == 1].prev_k();

    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (context.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t decoded_dy =
        context.dy_encoder[dy_instance].decode_int(streams[LASPP_CHANNEL_RETURNS_STREAM]);
    int32_t dy = wrapping_int32_add(decoded_dy, context.dy_streamed_median[mgps].get_median());
    context.dy_streamed_median[mgps].insert(dy);
    context.y = wrapping_int32_add(context.y, dy);

    uint_fast16_t dy_k = context.dy_encoder[dy_instance].prev_k();
    uint_fast16_t kxy_sum = (dx_k + dy_k) / 2;
    uint32_t kxy = static_cast<uint32_t>(kxy_sum);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (context.number_of_returns == 1) {
      dz_instance++;
    }
    int32_t decoded_dz = context.dz_encoder[dz_instance].decode_int(streams[LASPP_Z_STREAM]);
    context.z = wrapping_int32_add(context.last_z[context.l], decoded_dz);
    context.last_z[context.l] = context.z;

    if (streams.non_empty(LASPP_FLAGS_STREAM)) {  // TODO: Rest of streams
      uint8_t last_flags =
          static_cast<uint8_t>((context.edge_of_flight_line << 5) |
                               (context.scan_direction_flag << 4) | context.classification_flags);
      uint_fast16_t decoded_flags =
          context.flag_encoders[last_flags].decode_symbol(streams[LASPP_FLAGS_STREAM]);
      context.edge_of_flight_line = static_cast<uint8_t>((decoded_flags >> 5) & 0x1);
      context.scan_direction_flag = static_cast<uint8_t>((decoded_flags >> 4) & 0x1);
      context.classification_flags = static_cast<uint8_t>(decoded_flags & 0x0F);
    }

    if (streams.non_empty(LASPP_INTENSITY_STREAM)) {
      int32_t intensity_delta =
          context.intensity_encoder[context.cpr].decode_int(streams[LASPP_INTENSITY_STREAM]);
      int32_t new_intensity =
          static_cast<int32_t>(context.last_intensity[context.cprgps]) + intensity_delta;
      context.intensity = static_cast<uint16_t>(new_intensity);
      context.last_intensity[context.cprgps] = context.intensity;
    }

    if (streams.non_empty(LASPP_USER_DATA_STREAM)) {
      uint8_t user_ctx = context.user_data / 4;
      uint_fast16_t decoded_user_data =
          context.user_data_encoders[user_ctx].decode_symbol(streams[LASPP_USER_DATA_STREAM]);
      context.user_data = static_cast<uint8_t>(decoded_user_data);
    }

    return context;
  }

  void encode(LayeredOutStreams<NUM_LAYERS>& streams, const LASPointFormat6& point) {
    LASPointFormat6Context& prev_context = m_contexts[m_context];

    uint_fast8_t target_context_idx = point.scanner_channel;
    bool scanner_changed = target_context_idx != m_context;

    LASPointFormat6Context* reference_context = &prev_context;
    if (scanner_changed && m_contexts[target_context_idx].initialized) {
      reference_context = &m_contexts[target_context_idx];
    }

    uint_fast8_t old_cprgps = prev_context.cprgps;

    uint8_t last_number_of_returns = reference_context->number_of_returns;
    uint8_t last_return_number = reference_context->return_number;
    uint16_t prev_point_source_id = reference_context->point_source_id;
    double prev_gps_time = reference_context->gps_time;
    int16_t prev_scan_angle = reference_context->scan_angle;
    uint8_t prev_user_data = reference_context->user_data;
    uint8_t last_classification = static_cast<uint8_t>(reference_context->classification);
    uint8_t last_flags = static_cast<uint8_t>((reference_context->edge_of_flight_line << 5) |
                                              (reference_context->scan_direction_flag << 4) |
                                              reference_context->classification_flags);

    bool point_source_change = point.point_source_id != prev_point_source_id;
    bool gps_time_change = point.gps_time != prev_gps_time;
    bool scan_angle_change = point.scan_angle != prev_scan_angle;

    uint_fast16_t changed_values = 0;
    if (scanner_changed) {
      changed_values |= (1 << 6);
    }
    if (point_source_change) {
      changed_values |= (1 << 5);
    }
    if (gps_time_change) {
      changed_values |= (1 << 4);
    }
    if (scan_angle_change) {
      changed_values |= (1 << 3);
    }
    if (point.number_of_returns != last_number_of_returns) {
      changed_values |= (1 << 2);
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

    prev_context.changed_values_encoders[old_cprgps].encode_symbol(
        streams[LASPP_CHANNEL_RETURNS_STREAM], changed_values);

    if (scanner_changed) {
      uint_fast16_t delta = (point.scanner_channel - prev_context.scanner_channel + 4u) % 4u;
      prev_context.d_scanner_channel_encoder.encode_symbol(streams[LASPP_CHANNEL_RETURNS_STREAM],
                                                           delta - 1);
      if (!m_contexts[target_context_idx].initialized) {
        m_contexts[target_context_idx].initialize(prev_context);
        m_contexts[target_context_idx].scanner_channel =
            static_cast<uint8_t>(target_context_idx & 0x3);
      }
      m_context = target_context_idx;
      m_external_context = m_context;
    } else {
      // Workaround for issue with context setting in V3: https://github.com/LASzip/LASzip/issues/74
      m_external_context = Version == 3 ? 0 : m_context;
    }

    LASPointFormat6Context& context = m_contexts[m_context];

    if (changed_values & (1 << 2)) {
      context.n_returns_encoders[last_number_of_returns].encode_symbol(
          streams[LASPP_CHANNEL_RETURNS_STREAM], point.number_of_returns);
      context.number_of_returns = static_cast<uint8_t>(point.number_of_returns & 0xF);
    } else {
      context.number_of_returns = static_cast<uint8_t>(last_number_of_returns & 0xF);
    }

    uint_fast16_t return_flag = changed_values & 0b11;
    uint8_t new_return_number = last_return_number;
    if (return_flag == 0b01) {
      new_return_number = static_cast<uint8_t>((last_return_number + 1) & 0xF);
    } else if (return_flag == 0b10) {
      new_return_number = static_cast<uint8_t>((last_return_number + 15) & 0xF);
    } else if (return_flag == 0b11) {
      if (gps_time_change) {
        context.return_number_diff_time_encoder[last_return_number].encode_symbol(
            streams[LASPP_CHANNEL_RETURNS_STREAM], point.return_number);
        new_return_number = point.return_number;
      } else {
        context.d_return_number_same_time_encoder.encode_symbol(
            streams[LASPP_CHANNEL_RETURNS_STREAM], rn_diff - 2);
        new_return_number = static_cast<uint8_t>((last_return_number + rn_diff) & 0xF);
      }
    }
    context.return_number = static_cast<uint8_t>(new_return_number & 0xF);

    bool gps_changed = gps_time_change;
    context.set_cpr(gps_changed);
    context.set_m_l();

    if (point_source_change) {
      int32_t diff =
          static_cast<int32_t>(point.point_source_id) - static_cast<int32_t>(prev_point_source_id);
      context.point_source_id_encoder.encode_int(streams[LASPP_POINT_SOURCE_STREAM], diff);
    }
    context.point_source_id = point.point_source_id;

    if (gps_time_change) {
      context.gps_time_encoder.encode(streams[LASPP_GPS_TIME_STREAM], GPSTime(point.gps_time));
    }
    context.gps_time = point.gps_time;

    if (scan_angle_change) {
      int16_t prev_angle = prev_scan_angle;
      int16_t next_angle = point.scan_angle;
      int32_t diff = static_cast<int32_t>(next_angle) - static_cast<int32_t>(prev_angle);
      context.scan_angle_encoder[gps_changed].encode_int(streams[LASPP_SCAN_ANGLE_STREAM], diff);
    }
    context.scan_angle = point.scan_angle;

    uint8_t classification_idx =
        static_cast<uint8_t>(((last_classification & 0x1F) << 1) + (context.cpr == 3 ? 1 : 0));
    context.classification_encoders[classification_idx].encode_symbol(
        streams[LASPP_CLASSIFICATION_STREAM], static_cast<uint8_t>(point.classification));
    context.classification = point.classification;

    context.flag_encoders[last_flags].encode_symbol(
        streams[LASPP_FLAGS_STREAM],
        static_cast<uint8_t>((point.edge_of_flight_line << 5) | (point.scan_direction_flag << 4) |
                             point.classification_flags));
    context.edge_of_flight_line = point.edge_of_flight_line;
    context.scan_direction_flag = point.scan_direction_flag;
    context.classification_flags = point.classification_flags;

    int32_t intensity_base = static_cast<int32_t>(context.last_intensity[context.cprgps]);
    int32_t intensity_diff = static_cast<int32_t>(point.intensity) - intensity_base;
    context.intensity_encoder[context.cpr].encode_int(streams[LASPP_INTENSITY_STREAM],
                                                      intensity_diff);
    context.last_intensity[context.cprgps] = point.intensity;
    context.intensity = point.intensity;

    uint8_t user_ctx = prev_user_data / 4;
    context.user_data_encoders[user_ctx].encode_symbol(streams[LASPP_USER_DATA_STREAM],
                                                       point.user_data);
    context.user_data = point.user_data;

    uint32_t mgps = context.m * 2u + static_cast<uint32_t>(gps_changed);

    int32_t dx = wrapping_int32_sub(point.x, context.x);
    context.dx_encoders[context.number_of_returns == 1].encode_int(
        streams[LASPP_CHANNEL_RETURNS_STREAM],
        wrapping_int32_sub(dx, context.dx_streamed_median[mgps].get_median()));
    context.dx_streamed_median[mgps].insert(dx);
    context.x = point.x;

    uint_fast16_t dx_k = context.dx_encoders[context.number_of_returns == 1].prev_k();

    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (context.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy = wrapping_int32_sub(point.y, context.y);
    context.dy_encoder[dy_instance].encode_int(
        streams[LASPP_CHANNEL_RETURNS_STREAM],
        wrapping_int32_sub(dy, context.dy_streamed_median[mgps].get_median()));
    context.dy_streamed_median[mgps].insert(dy);
    context.y = point.y;

    uint_fast16_t dy_k = context.dy_encoder[dy_instance].prev_k();
    uint_fast16_t kxy_sum = (dx_k + dy_k) / 2;
    uint32_t kxy = static_cast<uint32_t>(kxy_sum);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (context.number_of_returns == 1) {
      dz_instance++;
    }
    int32_t dz = wrapping_int32_sub(point.z, context.last_z[context.l]);
    context.dz_encoder[dz_instance].encode_int(streams[LASPP_Z_STREAM], dz);
    context.last_z[context.l] = point.z;
    context.z = point.z;

    context.scanner_channel = static_cast<uint8_t>(point.scanner_channel & 0x3);
  }
};

using LASPointFormat6Encoder = LASPointFormat6EncoderBase<>;

}  // namespace laspp
