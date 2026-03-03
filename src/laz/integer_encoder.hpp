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

#include <limits>
#include <memory>
#include <optional>

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

    uint_fast16_t m_prev_k;

    uint_fast16_t decode(InStream& stream, uint8_t k) {
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
      return encoders_8_32.at(k - 8u).decode_symbol(stream);
    }

    void encode(OutStream& stream, uint_fast16_t k, uint_fast16_t val) {
      if (k < 8) {
        switch (k) {
          case 0:
            return encoder_0.encode_bit(stream, static_cast<bool>(val));
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
      return encoders_8_32.at(k - 8u).encode_symbol(stream, val);
    }
  };

 private:
  std::shared_ptr<SymbolEncoders> m_symbol_encoders;

 public:
  IntegerEncoder() : m_symbol_encoders(std::make_shared<SymbolEncoders>()) {}

  explicit IntegerEncoder(std::shared_ptr<SymbolEncoders> symbol_encoders)
      : m_symbol_encoders(std::move(symbol_encoders)) {}

  explicit IntegerEncoder(std::optional<std::shared_ptr<SymbolEncoders>> symbol_encoders)
      : m_symbol_encoders(symbol_encoders.value_or(std::make_shared<SymbolEncoders>())) {}

  int32_t decode_int(InStream& stream) {
    uint_fast16_t k = m_k_encoder.decode_symbol(stream);
    m_symbol_encoders->m_prev_k = k;
    if (k < 32) {
      int32_t val =
          static_cast<int32_t>(m_symbol_encoders->decode(stream, static_cast<uint8_t>(k)));
      if (k == 0) {
        return val;
      }
      if (k > 8) {
        int32_t lower_bits = static_cast<int32_t>(raw_decode(stream, static_cast<uint8_t>(k - 8)));
        val = (val << (k - 8)) | lower_bits;
      }
      if (val >= (1 << (k - 1))) {
        return val + 1;
      } else {
        return static_cast<int32_t>(static_cast<uint32_t>(val) - ((1u << k) - 1u));
      }
    } else {
      return std::numeric_limits<int32_t>::min();
    }
  }

  void encode_int(OutStream& stream, int32_t integer) {
    uint_fast16_t k = 0;
    while (integer > (static_cast<int64_t>(1) << k) ||
           integer < -((static_cast<int64_t>(1) << k) - 1)) {
      k++;
    }

    m_k_encoder.encode_symbol(stream, k);
    m_symbol_encoders->m_prev_k = k;

    if (k < 32) {
      if (k == 0) {
        m_symbol_encoders->encode(stream, k, static_cast<uint32_t>(integer));
        return;
      }
      if (integer < 0) {
        integer += static_cast<int32_t>((1u << k) - 1);
      } else {
        integer -= 1;
      }
      if (k <= 8) {
        m_symbol_encoders->encode(stream, k, static_cast<uint_fast16_t>(integer));
      } else {
        m_symbol_encoders->encode(stream, k, static_cast<uint_fast16_t>(integer >> (k - 8)));
        integer = integer & ((1 << (k - 8)) - 1);
        raw_encode(stream, static_cast<uint64_t>(integer), static_cast<uint8_t>(k - 8));
      }
    }
  }

  uint_fast16_t prev_k() const { return m_symbol_encoders->m_prev_k; }
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

  int32_t decode_int(uint32_t instance, InStream& stream) {
    return m_integer_encoders[instance].decode_int(stream);
  }

  IntegerEncoder<n_bits>& operator[](uint32_t instance) { return m_integer_encoders[instance]; }

  void encode_int(uint32_t instance, OutStream& stream, int32_t integer) {
    return this->operator[](instance).encode_int(stream, integer);
  }
};

}  // namespace laspp
