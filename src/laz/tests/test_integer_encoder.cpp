#include <cstring>
#include <sstream>

#include "laz/bit_symbol_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

using namespace laspp;

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

    {
      laspp::InStream instream(encoded_stream);
      laspp::SymbolEncoder<33> symbol_encoder;
      AssertEQ(symbol_encoder.decode_symbol(instream), 14);
      AssertEQ(symbol_encoder.decode_symbol(instream), 1);
      AssertEQ(symbol_encoder.decode_symbol(instream), 2);
      AssertEQ(symbol_encoder.decode_symbol(instream), 1);
      AssertEQ(symbol_encoder.decode_symbol(instream), 0);
      AssertEQ(symbol_encoder.decode_symbol(instream), 2);
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

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      AssertEQ(symbol_encoder.decode_bit(instream), 0);
      {
        laspp::IntegerEncoder<32> int_encoder;
        AssertEQ(int_encoder.decode_int(instream), 12442);
      }
      AssertEQ(symbol_encoder.decode_bit(instream), 1);
      AssertEQ(symbol_encoder.decode_bit(instream), 1);
      AssertEQ(laspp::raw_decode(instream, 36), 2445);
      AssertEQ(symbol_encoder.decode_bit(instream), 1);
      AssertEQ(symbol_encoder.decode_bit(instream), 0);
      AssertEQ(symbol_encoder.decode_bit(instream), 1);
    }
  }

  {
    std::stringstream encoded_stream;
    {
      laspp::IntegerEncoder<32> int_encoder;
      for (int32_t i = -1000; i < 1000; i++) {
        laspp::OutStream ostream(encoded_stream);
        int_encoder.encode_int(ostream, i);
      }
    }
    {
      laspp::IntegerEncoder<32> int_encoder;
      for (int32_t i = -1000; i < 1000; i++) {
        laspp::InStream instream(encoded_stream);
        AssertEQ(int_encoder.decode_int(instream), i);
      }
    }
  }

  return 0;
}
