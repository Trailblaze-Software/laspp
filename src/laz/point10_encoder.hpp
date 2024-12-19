/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
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

template <size_t N, typename T>
constexpr std::array<T, N> create_array(const T& val) {
  std::array<T, N> arr;
  arr.fill(val);
  return arr;
}

class LASPointFormat0Encoder {
  LASPointFormat0 m_last_las_point;

  SymbolEncoder<64> m_changed_values_encoder;
  std::array<SymbolEncoder<256>, 256> m_bit_byte_encoder;
  MultiInstanceIntegerEncoder<16, 4> m_intensity_encoder;
  std::array<uint16_t, 16> m_prev_intensities;
  std::array<SymbolEncoder<256>, 256> m_classification_encoder;
  std::array<SymbolEncoder<256>, 2> m_scan_angle_rank_encoder;
  std::array<SymbolEncoder<256>, 256> m_user_data_encoder;
  IntegerEncoder<16> m_point_source_id_encoder;
  MultiInstanceIntegerEncoder<32, 2> m_dx_encoder;
  std::array<StreamingMedian<int32_t>, 16> m_dx_streamed_median;
  MultiInstanceIntegerEncoder<32, 22> m_dy_encoder;
  std::array<StreamingMedian<int32_t>, 16> m_dy_streamed_median;
  MultiInstanceIntegerEncoder<32, 20> m_dz_encoder;
  std::array<int32_t, 8> m_prev_dz;  // Says 16 in spec?

  uint8_t m_m;

  static constexpr uint8_t return_map_m[8][8] = {
      {15, 14, 13, 12, 11, 10, 9, 8},  {14, 0, 1, 3, 6, 10, 10, 9},
      {13, 1, 2, 4, 7, 11, 11, 10},    {12, 3, 4, 5, 8, 12, 12, 11},
      {11, 6, 7, 8, 9, 13, 13, 12},    {10, 10, 11, 12, 13, 14, 14, 13},
      {9, 10, 11, 12, 13, 14, 15, 14}, {8, 9, 10, 11, 12, 13, 14, 15}};

 public:
  using EncodedType = LASPointFormat0;
  const LASPointFormat0& last_value() const { return m_last_las_point; }

  explicit LASPointFormat0Encoder(const LASPointFormat0& initial_las_point)
      : m_last_las_point(initial_las_point),
        m_prev_intensities(create_array<16, uint16_t>(0)),
        m_prev_dz(create_array<8>(0)) {
    m_m = return_map_m[m_last_las_point.bit_byte.number_of_returns]
                      [m_last_las_point.bit_byte.return_number];
  }

  LASPointFormat0 decode(InStream& stream) {
    uint_fast16_t changed_values = m_changed_values_encoder.decode_symbol(stream);
    if (changed_values) {
      if (changed_values & (1 << 5)) {
        m_last_las_point.bit_byte = static_cast<uint8_t>(
            m_bit_byte_encoder[m_last_las_point.bit_byte].decode_symbol(stream));

        m_m = return_map_m[m_last_las_point.bit_byte.number_of_returns]
                          [m_last_las_point.bit_byte.return_number];
      }

      if (changed_values & (1 << 4)) {
        if (m_m <= 2) {
          m_last_las_point.intensity =
              m_prev_intensities[m_m] +
              static_cast<uint16_t>(m_intensity_encoder.decode_int(m_m, stream));
        } else {
          m_last_las_point.intensity =
              m_prev_intensities[m_m] +
              static_cast<uint16_t>(m_intensity_encoder.decode_int(3, stream));
        }
        m_prev_intensities[m_m] = m_last_las_point.intensity;
      } else {
        m_last_las_point.intensity = m_prev_intensities[m_m];
      }

      if (changed_values & (1 << 3)) {
        m_last_las_point.classification_byte = static_cast<uint8_t>(
            m_classification_encoder[m_last_las_point.classification_byte].decode_symbol(stream));
      }
      if (changed_values & (1 << 2)) {
        m_last_las_point.scan_angle_rank += static_cast<uint8_t>(
            m_scan_angle_rank_encoder[m_last_las_point.bit_byte.scan_direction_flag].decode_symbol(
                stream));
      }
      if (changed_values & (1 << 1)) {
        m_last_las_point.user_data = static_cast<uint8_t>(
            m_user_data_encoder[m_last_las_point.user_data].decode_symbol(stream));
      }
      if (changed_values & 1) {
        m_last_las_point.point_source_id +=
            static_cast<uint16_t>(m_point_source_id_encoder.decode_int(stream));
      }
    }

    // X
    int32_t dx = m_dx_encoder[m_last_las_point.bit_byte.number_of_returns == 1].decode_int(stream) +
                 m_dx_streamed_median[m_m].get_median();
    m_dx_streamed_median[m_m].insert(dx);
    m_last_las_point.x = m_last_las_point.x + dx;

    uint_fast16_t dx_k = m_dx_encoder[m_last_las_point.bit_byte.number_of_returns == 1].prev_k();

    // Y
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (m_last_las_point.bit_byte.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy =
        m_dy_encoder[dy_instance].decode_int(stream) + m_dy_streamed_median[m_m].get_median();
    m_last_las_point.y = m_last_las_point.y + dy;
    m_dy_streamed_median[m_m].insert(dy);

    // Z
    uint32_t kxy = static_cast<uint32_t>((dx_k + m_dy_encoder[dy_instance].prev_k()) / 2);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (m_last_las_point.bit_byte.number_of_returns == 1) {
      dz_instance++;
    }

    int32_t dz = m_dz_encoder[dz_instance].decode_int(stream);
    uint32_t l = static_cast<uint32_t>(std::abs(m_last_las_point.bit_byte.number_of_returns -
                                                m_last_las_point.bit_byte.return_number));
    m_last_las_point.z = m_prev_dz[l] + dz;
    m_prev_dz[l] = m_last_las_point.z;

    return m_last_las_point;
  }

  void encode(OutStream& out_stream, const LASPointFormat0& las_point) {
    uint_fast16_t changed_values = 0;
    if (las_point.bit_byte != m_last_las_point.bit_byte) {
      changed_values |= (1 << 5);
      m_m = return_map_m[las_point.bit_byte.number_of_returns][las_point.bit_byte.return_number];
    }
    if (las_point.intensity != m_prev_intensities[m_m]) {
      changed_values |= (1 << 4);
    }
    if (las_point.classification_byte != m_last_las_point.classification_byte) {
      changed_values |= (1 << 3);
    }
    if (las_point.scan_angle_rank != m_last_las_point.scan_angle_rank) {
      changed_values |= (1 << 2);
    }
    if (las_point.user_data != m_last_las_point.user_data) {
      changed_values |= (1 << 1);
    }
    if (las_point.point_source_id != m_last_las_point.point_source_id) {
      changed_values |= 1;
    }

    m_changed_values_encoder.encode_symbol(out_stream, changed_values);

    if (changed_values) {
      if (changed_values & (1 << 5)) {
        m_bit_byte_encoder[m_last_las_point.bit_byte].encode_symbol(out_stream, las_point.bit_byte);
        m_last_las_point.bit_byte = las_point.bit_byte;
      }

      if (changed_values & (1 << 4)) {
        if (m_m <= 2) {
          m_intensity_encoder.encode_int(
              m_m, out_stream, static_cast<int16_t>(las_point.intensity - m_prev_intensities[m_m]));
        } else {
          m_intensity_encoder.encode_int(
              3, out_stream, static_cast<int16_t>(las_point.intensity - m_prev_intensities[m_m]));
        }
        m_prev_intensities[m_m] = las_point.intensity;
        m_last_las_point.intensity = las_point.intensity;
      }

      if (changed_values & (1 << 3)) {
        m_classification_encoder[m_last_las_point.classification_byte].encode_symbol(
            out_stream, las_point.classification_byte);
        m_last_las_point.classification_byte = las_point.classification_byte;
      }
      if (changed_values & (1 << 2)) {
        m_scan_angle_rank_encoder[m_last_las_point.bit_byte.scan_direction_flag].encode_symbol(
            out_stream,
            static_cast<uint8_t>(las_point.scan_angle_rank - m_last_las_point.scan_angle_rank));
        m_last_las_point.scan_angle_rank = las_point.scan_angle_rank;
      }
      if (changed_values & (1 << 1)) {
        m_user_data_encoder[m_last_las_point.user_data].encode_symbol(out_stream,
                                                                      las_point.user_data);
        m_last_las_point.user_data = las_point.user_data;
      }
      if (changed_values & 1) {
        m_point_source_id_encoder.encode_int(
            out_stream, las_point.point_source_id - m_last_las_point.point_source_id);
        m_last_las_point.point_source_id = las_point.point_source_id;
      }
    }

    // X
    int32_t dx = las_point.x - m_last_las_point.x;
    m_dx_encoder[m_last_las_point.bit_byte.number_of_returns == 1].encode_int(
        out_stream, dx - m_dx_streamed_median[m_m].get_median());
    m_dx_streamed_median[m_m].insert(dx);
    m_last_las_point.x = las_point.x;

    uint_fast16_t dx_k = m_dx_encoder[m_last_las_point.bit_byte.number_of_returns == 1].prev_k();

    // Y
    uint32_t dy_instance = (dx_k < 20) ? (dx_k & (~1u)) : 20;
    if (m_last_las_point.bit_byte.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy = las_point.y - m_last_las_point.y;
    m_dy_encoder[dy_instance].encode_int(out_stream, dy - m_dy_streamed_median[m_m].get_median());
    m_last_las_point.y = las_point.y;
    m_dy_streamed_median[m_m].insert(dy);

    // Z
    uint32_t kxy = static_cast<uint32_t>((dx_k + m_dy_encoder[dy_instance].prev_k()) / 2);
    uint32_t dz_instance = (kxy < 18) ? (kxy & (~1u)) : 18;
    if (m_last_las_point.bit_byte.number_of_returns == 1) {
      dz_instance++;
    }
    uint32_t l = static_cast<uint32_t>(std::abs(m_last_las_point.bit_byte.number_of_returns -
                                                m_last_las_point.bit_byte.return_number));
    int32_t dz = las_point.z - m_prev_dz[l];
    m_dz_encoder[dz_instance].encode_int(out_stream, dz);
    m_prev_dz[l] = las_point.z;
    m_last_las_point.z = las_point.z;
  }
};

}  // namespace laspp
