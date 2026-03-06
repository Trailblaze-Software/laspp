/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <cstring>
#include <sstream>

#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      symbol_encoder.encode_symbol(ostream, 14);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 2);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 0);
      symbol_encoder.encode_symbol(ostream, 2);
    }

    {
      laspp::InStream instream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 14);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 2);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 1);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 0);
      LASPP_ASSERT_EQ(symbol_encoder.decode_symbol(instream), 2);
    }
  }

  {
    std::stringstream encoded_stream;
    {
      std::vector<uint16_t> symbols;

      for (int i = 0; i < (1 << 16); i++) {
        symbols.push_back(static_cast<uint16_t>(i % 33));
      }

      {
        laspp::OutStream ostream(encoded_stream);
        laspp::SymbolEncoder<33> symbol_encoder;
        for (const auto& symbol : symbols) {
          symbol_encoder.encode_symbol(ostream, symbol);
        }
      }

      {
        laspp::InStream instream(encoded_stream);
        laspp::SymbolEncoder<33> symbol_decoder;
        for (const auto& symbol : symbols) {
          LASPP_ASSERT_EQ(symbol_decoder.decode_symbol(instream), symbol);
        }
      }
    }
  }

  {
    std::stringstream encoded_stream;
    {
      std::vector<uint16_t> symbols;

      for (int i = 0; i < 10000; i++) {
        symbols.push_back(static_cast<uint16_t>(i % 1023));
      }

      {
        laspp::OutStream ostream(encoded_stream);
        laspp::SymbolEncoder<1023> symbol_encoder;
        for (const auto& symbol : symbols) {
          symbol_encoder.encode_symbol(ostream, symbol);
        }
      }

      {
        laspp::InStream instream(encoded_stream);
        laspp::SymbolEncoder<1023> symbol_decoder;
        for (const auto& symbol : symbols) {
          LASPP_ASSERT_EQ(symbol_decoder.decode_symbol(instream), symbol);
        }
      }
    }
  }

  return 0;
}
