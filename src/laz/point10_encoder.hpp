#pragma once

#include <bitset>
#include <cstdint>

#include "laspoint.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/streaming_median.hpp"

namespace laspp {
class LASPointFormat0Encoder {
  LASPointFormat0 m_last_las_point;

  SymbolEncoder<64> m_changed_values_encoder;
  std::array<SymbolEncoder<256>, 256> m_bit_byte_encoder;
  MultiInstanceIntegerEncoder<16, 4> m_intensity_encoder;
  std::array<int16_t, 16> m_prev_intensities;
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

 public:
  explicit LASPointFormat0Encoder(const LASPointFormat0& initial_las_point)
      : m_last_las_point(initial_las_point) {}

  LASPointFormat0 decompress(InStream& stream) {
    uint8_t changed_values = m_changed_values_encoder.decode_symbol(stream);
    std::cout << "Changed values: " << std::bitset<6>(changed_values) << std::endl;
    LASPointFormat0 las_point;
    if (changed_values & (1 << 5)) {
      las_point.bit_byte = m_bit_byte_encoder[m_last_las_point.bit_byte].decode_symbol(stream);
    } else {
      las_point.bit_byte = m_last_las_point.bit_byte;
    }

    static uint8_t return_map_m[8][8] = {
        {15, 14, 13, 12, 11, 10, 9, 8},  {14, 0, 1, 3, 6, 10, 10, 9},
        {13, 1, 2, 4, 7, 11, 11, 10},    {12, 3, 4, 5, 8, 12, 12, 11},
        {11, 6, 7, 8, 9, 13, 13, 12},    {10, 10, 11, 12, 13, 14, 14, 13},
        {9, 10, 11, 12, 13, 14, 15, 14}, {8, 9, 10, 11, 12, 13, 14, 15}};

    uint8_t m =
        return_map_m[las_point.bit_byte.number_of_returns][las_point.bit_byte.return_number];
    uint8_t l = std::abs(las_point.bit_byte.number_of_returns - las_point.bit_byte.return_number);

    if (changed_values & (1 << 4)) {
      if (m <= 2) {
        las_point.intensity = m_prev_intensities[m] + m_intensity_encoder.decode_int(m, stream);
      } else {
        las_point.intensity = m_prev_intensities[m] + m_intensity_encoder.decode_int(3, stream);
      }
      m_prev_intensities[m] = las_point.intensity;
    } else {
      las_point.intensity = m_prev_intensities[m];
    }

    if (changed_values & (1 << 3)) {
      las_point.classification_byte =
          m_classification_encoder[m_last_las_point.classification_byte].decode_symbol(stream);
    } else {
      las_point.classification_byte = m_last_las_point.classification_byte;
    }

    if (changed_values & (1 << 2)) {
      las_point.scan_angle_rank =
          m_scan_angle_rank_encoder[las_point.bit_byte.scan_direction_flag].decode_symbol(stream);
    } else {
      las_point.scan_angle_rank = m_last_las_point.scan_angle_rank;
    }

    if (changed_values & (1 << 1)) {
      las_point.user_data =
          m_user_data_encoder[(uint8_t)m_last_las_point.user_data].decode_symbol(stream);
    } else {
      las_point.user_data = m_last_las_point.user_data;
    }

    if (changed_values & 1) {
      las_point.point_source_id =
          m_last_las_point.point_source_id + m_point_source_id_encoder.decode_int(stream);
    } else {
      las_point.point_source_id = m_last_las_point.point_source_id;
    }

    int32_t dx = m_dx_encoder[las_point.bit_byte.number_of_returns == 1].decode_int(stream) +
                 m_dx_streamed_median[m].get_median();
    las_point.x = m_last_las_point.x + dx;
    m_dx_streamed_median[m].insert(dx);

    uint8_t dx_k = m_dx_encoder[las_point.bit_byte.number_of_returns == 1].prev_k();
    uint8_t dy_instance = (dx_k < 20) ? (dx_k ^ 1) : 20;
    if (las_point.bit_byte.number_of_returns == 1) {
      dy_instance++;
    }
    int32_t dy =
        m_dy_encoder[dy_instance].decode_int(stream) + m_dy_streamed_median[m].get_median();
    las_point.y = m_last_las_point.y + dy;
    m_dy_streamed_median[m].insert(dy);

    uint8_t kxy = (dx_k + m_dy_encoder[las_point.bit_byte.number_of_returns == 1].prev_k()) / 2;
    uint8_t dz_instance = (kxy < 18) ? (kxy ^ 1) : 18;
    if (las_point.bit_byte.number_of_returns == 1) {
      dz_instance++;
    }
    las_point.z = m_prev_dz[l] + m_dz_encoder[dz_instance].decode_int(stream);
    m_prev_dz[l] = las_point.z;

    m_last_las_point = las_point;
    return las_point;
  }
};

}  // namespace laspp
