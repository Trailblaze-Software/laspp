#pragma once

#include <limits>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

namespace laspp {

template <uint8_t n_bits>
class IntegerEncoder {
  SymbolEncoder<n_bits + 1> m_k_encoder;

  struct SymbolEncoders {
    BitSymbolEncoder encoder_0;
    SymbolEncoder<2> encoder_1;
    SymbolEncoder<4> encoder_2;
    SymbolEncoder<8> encoder_3;
    SymbolEncoder<16> encoder_4;
    SymbolEncoder<32> encoder_5;
    SymbolEncoder<64> encoder_6;
    SymbolEncoder<128> encoder_7;
    std::array<SymbolEncoder<256>, 25> encoders_8_32;

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
        __builtin_unreachable();
      } else {
        return encoders_8_32.at(k - 8).decode_symbol(stream);
      }
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
        __builtin_unreachable();
      } else {
        return encoders_8_32.at(k - 8).encode_symbol(stream, val);
      }
    }
  } m_symbol_encoders;

 public:
  IntegerEncoder() {}

  int32_t decode_int(InStream& stream) {
    uint8_t k = m_k_encoder.decode_symbol(stream);
    if (k < 32) {
      int32_t val = m_symbol_encoders.decode(stream, k);
      if (k > 8) {
        int32_t lower_bits = raw_decode(stream, k - 8);
        val = (val << (k - 8)) + lower_bits;
      }
      if (val >= (1 << (k - 1))) {
        return val + 1;
      } else {
        return val - (1 << k) - 1;
      }
    } else {
      return std::numeric_limits<int32_t>::min();
    }
  }

  void encode_int(OutStream& stream, int32_t integer) {
    uint8_t k = 0;
    while (integer > (1l << k) || integer < -((1l << k) - 1)) {
      k++;
    }

    m_k_encoder.encode_symbol(stream, k);

    if (k < 32) {
      if (integer < 0) {
        integer += (1ul << k) - 1;
      } else {
        integer -= 1;
      }
      if (k <= 8) {
        m_symbol_encoders.encode(stream, k, integer);
      } else {
        m_symbol_encoders.encode(stream, k, integer >> (k - 8));
        integer = integer & ((1 << (k - 8)) - 1);
        raw_encode(stream, integer, k - 8);
      }
    }
  }
};

}  // namespace laspp
