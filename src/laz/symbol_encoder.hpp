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
#include <cassert>
#include <cstdint>
#include <numeric>
#include <vector>

#include "laz/stream.hpp"

namespace laspp {

template <typename T, uint32_t Size>
struct ConstSizeVector {
  std::vector<T> data;

  ConstSizeVector() : data(Size) {}

  T& operator[](uint32_t idx) { return data[idx]; }
};

template <uint16_t NSymbols>
class SymbolEncoder {
  static_assert(NSymbols > 1, "Number of symbols must be greater than 1");
  static_assert(NSymbols < 1024, "Number of symbols must be less than 1024");
  enum class LookupTableType { NONE, ARRAY };

  static constexpr LookupTableType lookup_table_type =
      NSymbols < 16 ? LookupTableType::NONE : LookupTableType::ARRAY;

  template <LookupTableType type, uint16_t NumSymbols>
  struct LookupTable;

  template <uint16_t NumSymbols>
  struct LookupTable<LookupTableType::NONE, NumSymbols> {};

  template <uint16_t NumSymbols>
  struct LookupTable<LookupTableType::ARRAY, NumSymbols> {
    static constexpr uint_fast16_t lookup_table_n_bits(uint16_t N_symbols) {
      uint_fast16_t bits = 0;
      while ((1ull << bits) < N_symbols) {
        bits++;
      }
      return bits - 2;
    }

    static constexpr uint32_t N_BITS = lookup_table_n_bits(NSymbols);
    static_assert(N_BITS < 15, "Lookup table must use less than 15 bits");
    static constexpr uint32_t SHIFT = 15 - N_BITS;
    static constexpr uint32_t SIZE = (1u << N_BITS) + 2;

    using arr_type = typename std::conditional<(SIZE < 128), std::array<uint16_t, SIZE>,
                                               ConstSizeVector<uint16_t, SIZE>>::type;

    arr_type table;

    constexpr uint32_t size() const { return SIZE; }

    uint16_t& operator[](uint32_t idx) { return table[idx]; }
  };

  std::array<uint16_t, NSymbols> symbol_count;
  std::array<uint16_t, NSymbols> distribution;

  LookupTable<lookup_table_type, NSymbols> lookup_table;

  uint32_t update_cycle;
  uint32_t symbols_until_update;

  void update_distribution() {
    uint32_t symbol_sum = std::accumulate(symbol_count.begin(), symbol_count.end(), 0u);
    if (symbol_sum > (1 << 15)) {
      symbol_sum = 0;
      for (size_t s = 0; s < NSymbols; s++) {
        symbol_count[s] = static_cast<uint16_t>((symbol_count[s] + 1) / 2);
        symbol_sum += symbol_count[s];
      }
    }
    assert(symbol_sum >= NSymbols && symbol_sum > 0);
    uint32_t scale_factor = (1u << 31) / symbol_sum;
    uint32_t cumulitive_sum = 0;
    uint32_t lookup_idx = 0;
    if constexpr (lookup_table_type != LookupTableType::NONE) {
      lookup_table[lookup_idx++] = 0;
    }
    for (uint_fast16_t s = 0; s < NSymbols; s++) {
      distribution[s] = static_cast<uint16_t>((scale_factor * cumulitive_sum) / (1u << 16));
      cumulitive_sum += symbol_count[s];

      if constexpr (lookup_table_type != LookupTableType::NONE) {
        uint32_t shifted_dist = distribution[s] >> lookup_table.SHIFT;
        while (lookup_idx < shifted_dist + 1) {
          lookup_table[lookup_idx++] = static_cast<uint16_t>(s - 1);
        }
      }
    }
    if constexpr (lookup_table_type != LookupTableType::NONE) {
      while (lookup_idx < lookup_table.size()) {
        lookup_table[lookup_idx++] = NSymbols - 1;
      }
    }
    update_cycle = std::min((5 * update_cycle) / 4, 8u * (NSymbols + 6));
    symbols_until_update = update_cycle;
  }

 public:
  SymbolEncoder()
      : symbol_count(), distribution(), lookup_table(), update_cycle(), symbols_until_update() {
    for (size_t s = 0; s < NSymbols; s++) {
      symbol_count[s] = 1;
    }
    update_distribution();
    update_cycle = (NSymbols + 6) / 2;
    symbols_until_update = update_cycle;
  }

  uint_fast16_t decode_symbol(InStream& stream) {
    uint32_t value = stream.get_value();
    uint32_t l_tmp = (stream.length() >> 15);
    uint_fast16_t symbol = 0;

    if constexpr (lookup_table_type == LookupTableType::NONE) {
      for (; symbol + 1 < NSymbols && distribution[symbol + 1] * l_tmp <= value; symbol++) {
      }
    } else {
      uint32_t lookup_idx = (value / l_tmp) >> lookup_table.SHIFT;

      symbol = lookup_table[lookup_idx];
      for (; symbol + 1 < NSymbols && symbol + 1 <= lookup_table[lookup_idx + 1] &&
             distribution[symbol + 1] * l_tmp <= value;
           symbol++) {
      }
    }

    stream.update_range(distribution[symbol] * l_tmp, symbol < NSymbols - 1
                                                          ? (distribution[symbol + 1] * l_tmp)
                                                          : stream.length());

    symbol_count[symbol]++;
    if (--symbols_until_update == 0) {
      update_distribution();
    }
    return symbol;
  }

  void encode_symbol(OutStream& stream, uint_fast16_t symbol) {
    LASPP_ASSERT_LT(symbol, NSymbols);
    uint32_t l_tmp = (stream.length() >> 15);
    stream.update_range(distribution[symbol] * l_tmp, symbol < NSymbols - 1
                                                          ? (distribution[symbol + 1] * l_tmp)
                                                          : stream.length());

    symbol_count[symbol]++;
    symbols_until_update--;
    if (symbols_until_update == 0) {
      update_distribution();
    }
  }

  friend std::ostream& operator<<(std::ostream& os, const SymbolEncoder& encoder) {
    os << "Update cycle: " << encoder.update_cycle << std::endl;
    os << "Symbols until update: " << encoder.symbols_until_update << std::endl;
    for (size_t s = 0; s < NSymbols; s++) {
      os << "Symbol " << s << " count: " << encoder.symbol_count[s] << std::endl;
      os << "Symbol " << s << " distribution: " << encoder.distribution[s] << std::endl;
    }
    return os;
  }
};

}  // namespace laspp
