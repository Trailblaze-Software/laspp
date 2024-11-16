#pragma once

#include <cstdint>

#include "laz/stream.hpp"

namespace laspp {

class BitSymbolEncoder {
  uint16_t bit_0_count;
  uint16_t bit_count;
  uint16_t bit_0_prob;
  uint16_t update_cycle;
  uint16_t bits_until_update;

  void update_distribution() {
    bit_count += update_cycle;

    if (bit_count > 1 << 13) {
      bit_count = (bit_count + 1) / 2;
      bit_0_count = (bit_0_count + 1) / 2;
      if (bit_0_count == bit_count) {
        bit_count++;
      }
    }
    bit_0_prob = ((1u << 31) / bit_count) * bit_0_count / (1u << 18);
    update_cycle = std::min((5 * update_cycle) / 4, 64);
    bits_until_update = update_cycle;
  }

 public:
  BitSymbolEncoder()
      : bit_0_count(0), bit_count(2), bit_0_prob(4096), update_cycle(4), bits_until_update(4) {}

  uint16_t decode_bit(InStream& stream) {
    uint32_t value = stream.get_value();

    uint32_t mid = bit_0_prob * (stream.length() / (1u << 13));
    bool bit = value >= mid;

    stream.update_range(bit ? mid : 0, bit ? stream.length() : mid);

    if (!bit) bit_0_count++;
    bits_until_update--;
    if (bits_until_update == 0) {
      update_distribution();
    }

    stream.get_value();

    return bit;
  }

  void encode_bit(OutStream& stream, bool bit) {
    uint32_t mid = bit_0_prob * (stream.length() / (1u << 13));
    stream.update_range(bit ? mid : 0, bit ? stream.length() : mid);

    if (!bit) bit_0_count++;
    bits_until_update--;
    if (bits_until_update == 0) {
      update_distribution();
    }
    stream.get_base();
  }
};

}  // namespace laspp
