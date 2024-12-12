#include <cstring>
#include <iostream>
#include <sstream>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

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

    std::cout << "Encoded stream:";
    for (char c : encoded_stream.str()) {
      std::cout << " " << (int32_t)c;
    }
    std::cout << std::endl;

    {
      laspp::InStream instream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      for (size_t i = 0; i < 6; i++) {
        uint32_t symbol = symbol_encoder.decode_symbol(instream);
        std::cout << "Symbol: " << symbol << std::endl;
        ;
      }
    }
  }

  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      symbol_encoder.encode_bit(ostream, 0);

      laspp::IntegerEncoder<32> int_encoder;
      int_encoder.encode_int(ostream, 12442);

      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 1);
      laspp::raw_encode(ostream, 2445, 36);
      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 0);
      symbol_encoder.encode_bit(ostream, 1);
    }

    std::cout << "Encoded stream:";
    for (char c : encoded_stream.str()) {
      std::cout << " " << (int32_t)c;
    }
    std::cout << std::endl;

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      uint32_t bit = symbol_encoder.decode_bit(instream);
      std::cout << "Symbol: " << bit << std::endl;
      {
        laspp::IntegerEncoder<32> int_encoder;
        std::cout << "Decoded int " << int_encoder.decode_int(instream) << std::endl;
      }
      for (size_t i = 0; i < 2; i++) {
        uint32_t bit = symbol_encoder.decode_bit(instream);
        std::cout << "Symbol: " << bit << std::endl;
        ;
      }
      std::cout << "Raw: " << laspp::raw_decode(instream, 36) << std::endl;
      for (size_t i = 0; i < 3; i++) {
        uint32_t bit = symbol_encoder.decode_bit(instream);
        std::cout << "Symbol: " << bit << std::endl;
        ;
      }
    }
  }

  return 0;
}
