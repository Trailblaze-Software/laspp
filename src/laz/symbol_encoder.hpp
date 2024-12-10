#pragma once

#include <array>
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
      while ((1u << bits) < N_symbols) {
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

  uint16_t update_cycle;
  uint16_t symbols_until_update;

  void update_distribution() {
    uint16_t symbol_sum = std::accumulate(symbol_count.begin(), symbol_count.end(), 0);
    if (symbol_sum > (1 << 15)) {
      symbol_sum = 0;
      for (size_t s = 0; s < NSymbols; s++) {
        symbol_count[s] = (symbol_count[s] + 1) / 2;
        symbol_sum += symbol_count[s];
      }
    }
    uint32_t scale_factor = (1u << 31) / symbol_sum;
    uint32_t cumulitive_sum = 0;
    uint32_t lookup_idx = 0;
    if constexpr (lookup_table_type != LookupTableType::NONE) {
      lookup_table[lookup_idx++] = 0;
    }
    for (uint32_t s = 0; s < NSymbols; s++) {
      distribution[s] = (scale_factor * cumulitive_sum) / (1 << 16);
      cumulitive_sum += symbol_count[s];

      if constexpr (lookup_table_type != LookupTableType::NONE) {
        uint32_t shifted_dist = distribution[s] >> lookup_table.SHIFT;
        while (lookup_idx < shifted_dist + 1) {
          lookup_table[lookup_idx++] = s - 1;
        }
      }
    }
    if constexpr (lookup_table_type != LookupTableType::NONE) {
      while (lookup_idx < lookup_table.size()) {
        lookup_table[lookup_idx++] = NSymbols - 1;
      }
    }
    update_cycle = std::min((5 * update_cycle) / 4, 8 * (NSymbols + 6));
    symbols_until_update = update_cycle;
  }

 public:
  SymbolEncoder() {
    for (size_t s = 0; s < NSymbols; s++) {
      symbol_count[s] = 1;
    }
    update_distribution();
    update_cycle = (NSymbols + 6) / 2;
    symbols_until_update = update_cycle;
  }

  uint16_t decode_symbol(InStream& stream) {
    uint32_t value = stream.get_value();
    uint32_t l_tmp = (stream.length() >> 15);
    uint16_t symbol = 0;

    if constexpr (lookup_table_type == LookupTableType::NONE) {
      for (size_t s = symbol + 1; s < NSymbols && distribution[s] * l_tmp <= value; symbol = s++) {
      }
    } else {
      uint32_t lookup_idx = (value / l_tmp) >> lookup_table.SHIFT;

      symbol = lookup_table[lookup_idx];
      for (uint32_t s = symbol + 1;
           s < NSymbols && s <= lookup_table[lookup_idx + 1] && distribution[s] * l_tmp <= value;
           symbol = s++) {
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

  void encode_symbol(OutStream& stream, uint16_t symbol) {
    uint32_t l_tmp = (stream.length() >> 15);

    stream.update_range(distribution[symbol] * l_tmp, symbol < NSymbols - 1
                                                          ? (distribution[symbol + 1] * l_tmp)
                                                          : stream.length());

    symbol_count[symbol]++;
    symbols_until_update--;
    if (symbols_until_update == 0) {
      update_distribution();
    }
    stream.get_base();
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
