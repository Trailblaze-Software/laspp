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

#include "laz/stream.hpp"

namespace laspp {

class BitSymbolEncoder {
  int32_t bit_0_count;
  int32_t bit_count;
  int32_t bit_0_prob;
  int32_t update_cycle;
  int32_t bits_until_update;

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
      : bit_0_count(1), bit_count(2), bit_0_prob(4096), update_cycle(4), bits_until_update(4) {}

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
    stream.get_base();
    uint32_t mid = bit_0_prob * (stream.length() / (1u << 13));
    stream.update_range(bit ? mid : 0, bit ? stream.length() : mid);

    if (!bit) bit_0_count++;
    bits_until_update--;
    if (bits_until_update == 0) {
      update_distribution();
    }
  }
};

}  // namespace laspp
