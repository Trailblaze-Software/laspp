#pragma once

#include <array>
#include <cstdint>
#include <numeric>

#include "laz/stream.hpp"
#include "utilities/assert.hpp"

namespace laspp {

template <uint16_t NSymbols>
class SymbolEncoder {
  std::array<uint16_t, NSymbols> symbol_count;
  std::array<uint16_t, NSymbols> distribution;
  uint16_t update_cycle;
  uint16_t symbols_until_update;

  void update_distribution() {
    uint16_t symbol_sum = std::accumulate(symbol_count.begin(), symbol_count.end(), 0);
    if (symbol_sum > 1 << 15) {
      symbol_sum = 0;
      for (size_t s = 0; s < NSymbols; s++) {
        symbol_count[s] = (symbol_count[s] + 1) / 2;
        symbol_sum += symbol_count[s];
      }
    }
    uint32_t scale = (1u << 31) / symbol_sum;
    uint32_t sum = 0;
    for (size_t s = 0; s < NSymbols; s++) {
      distribution[s] = (scale * sum) / (1 << 16);
      sum += symbol_count[s];
    }
    update_cycle = std::min((5 * update_cycle) / 4, 8 * NSymbols + 6);
    symbols_until_update = update_cycle;
  }

 public:
  SymbolEncoder() : update_cycle((NSymbols + 6) / 2) {
    for (size_t s = 0; s < NSymbols; s++) {
      symbol_count[s] = 1;
    }
    update_distribution();
    symbols_until_update = update_cycle;
  }

  uint16_t decode_symbol(InStream& stream) {
    uint32_t value = stream.get_value();
    uint32_t l_tmp = (stream.length() >> 15);
    uint16_t symbol;

    for (size_t s = 0; s < NSymbols; s++) {
      if (distribution[s] * l_tmp <= value) {
        symbol = s;
      }
    }

    stream.update_range(distribution[symbol] * l_tmp, symbol < NSymbols - 1
                                                          ? (distribution[symbol + 1] * l_tmp)
                                                          : stream.length());

    symbol_count[symbol]++;
    symbols_until_update--;
    if (symbols_until_update == 0) {
      update_distribution();
    }

    stream.get_value();

    return symbol;
  }

  void encode_symbol(OutStream& stream, uint16_t symbol) {
    AssertGT(NSymbols, symbol);
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
};

}  // namespace laspp
