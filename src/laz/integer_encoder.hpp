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

#include <limits>
#include <memory>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

namespace laspp {

template <uint8_t n_bits, typename = std::enable_if_t<n_bits == 8 || n_bits == 16 || n_bits == 32>>
class IntegerEncoder {
  SymbolEncoder<n_bits + 1> m_k_encoder;

 public:
  struct SymbolEncoders {
    BitSymbolEncoder encoder_0;
    SymbolEncoder<2> encoder_1;
    SymbolEncoder<4> encoder_2;
    SymbolEncoder<8> encoder_3;
    SymbolEncoder<16> encoder_4;
    SymbolEncoder<32> encoder_5;
    SymbolEncoder<64> encoder_6;
    SymbolEncoder<128> encoder_7;
    std::array<SymbolEncoder<256>, n_bits - 7> encoders_8_32;

    uint8_t m_prev_k;

    uint8_t decode(InStream& stream, uint8_t k) {
      if (k < 8) {
        switch (k) {
          case 0:
            return encoder_0.decode_bit(stream);
          case 1:
            return encoder_1.decode_symbol(stream);
          case 2:
            return encoder_2.decode_symbol(stream);
          case 3:
            return encoder_3.decode_symbol(stream);
          case 4:
            return encoder_4.decode_symbol(stream);
          case 5:
            return encoder_5.decode_symbol(stream);
          case 6:
            return encoder_6.decode_symbol(stream);
          case 7:
            return encoder_7.decode_symbol(stream);
        }
      }
      return encoders_8_32.at(k - 8).decode_symbol(stream);
    }

    void encode(OutStream& stream, uint8_t k, uint8_t val) {
      if (k < 8) {
        switch (k) {
          case 0:
            return encoder_0.encode_bit(stream, val);
          case 1:
            return encoder_1.encode_symbol(stream, val);
          case 2:
            return encoder_2.encode_symbol(stream, val);
          case 3:
            return encoder_3.encode_symbol(stream, val);
          case 4:
            return encoder_4.encode_symbol(stream, val);
          case 5:
            return encoder_5.encode_symbol(stream, val);
          case 6:
            return encoder_6.encode_symbol(stream, val);
          case 7:
            return encoder_7.encode_symbol(stream, val);
        }
      }
      return encoders_8_32.at(k - 8).encode_symbol(stream, val);
    }
  };

 private:
  std::shared_ptr<SymbolEncoders> m_symbol_encoders;

 public:
  explicit IntegerEncoder(std::optional<std::shared_ptr<SymbolEncoders>> symbol_encoders = {})
      : m_symbol_encoders(symbol_encoders.has_value() ? symbol_encoders.value()
                                                      : std::make_shared<SymbolEncoders>()) {}

  int32_t decode_int(InStream& stream) {
    uint8_t k = m_k_encoder.decode_symbol(stream);
    m_symbol_encoders->m_prev_k = k;
    if (k < 32) {
      int32_t val = m_symbol_encoders->decode(stream, k);
      if (k == 0) {
        return val;
      }
      if (k > 8) {
        int32_t lower_bits = raw_decode(stream, k - 8);
        val = (val << (k - 8)) | lower_bits;
      }
      if (val >= (1 << (k - 1))) {
        return val + 1;
      } else {
        return val - ((1 << k) - 1);
      }
    } else {
      return std::numeric_limits<int32_t>::min();
    }
  }

  void encode_int(OutStream& stream, int32_t integer) {
    uint8_t k = 0;
    while (integer > (static_cast<int64_t>(1) << k) ||
           integer < -((static_cast<int64_t>(1) << k) - 1)) {
      k++;
    }

    m_k_encoder.encode_symbol(stream, k);
    m_symbol_encoders->m_prev_k = k;

    if (k < 32) {
      if (k == 0) {
        m_symbol_encoders->encode(stream, k, integer);
        return;
      }
      if (integer < 0) {
        integer += (1ull << k) - 1;
      } else {
        integer -= 1;
      }
      if (k <= 8) {
        m_symbol_encoders->encode(stream, k, integer);
      } else {
        m_symbol_encoders->encode(stream, k, integer >> (k - 8));
        integer = integer & ((1 << (k - 8)) - 1);
        raw_encode(stream, integer, k - 8);
      }
    }
  }

  uint8_t prev_k() const { return m_symbol_encoders->m_prev_k; }
};

template <uint8_t n_bits, uint8_t n_instances>
class MultiInstanceIntegerEncoder {
  std::shared_ptr<typename IntegerEncoder<n_bits>::SymbolEncoders> m_symbol_encoders;
  std::array<IntegerEncoder<n_bits>, n_instances> m_integer_encoders;

 public:
  MultiInstanceIntegerEncoder()
      : m_symbol_encoders(std::make_shared<typename IntegerEncoder<n_bits>::SymbolEncoders>()) {
    for (size_t i = 0; i < n_instances; i++) {
      m_integer_encoders[i] = IntegerEncoder<n_bits>(m_symbol_encoders);
    }
  }

  int32_t decode_int(uint8_t instance, InStream& stream) {
    return m_integer_encoders[instance].decode_int(stream);
  }

  void encode_int(uint8_t instance, OutStream& stream, int32_t integer) {
    return m_integer_encoders[instance].encode_int(stream, integer);
  }

  IntegerEncoder<n_bits>& operator[](uint8_t instance) { return m_integer_encoders[instance]; }
};

}  // namespace laspp
