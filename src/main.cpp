#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "las_point.hpp"
#include "las_reader.hpp"
#include "laz/bit_symbol_encoder.hpp"
#include "laz/integer_encoder.hpp"
#include "laz/raw_encoder.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

class LASPoint {
  std::array<int32_t, 3> position;

 public:
  LASPoint(const laspp::LASPointFormat0& point) : position({point.x, point.y, point.z}) {}

  LASPoint() = default;

  friend std::ostream& operator<<(std::ostream& os, const LASPoint& point) {
    os << "Position: (" << point.position[0] << ", " << point.position[1] << ", "
       << point.position[2] << ")";
    return os;
  }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  std::string filename(argv[1]);

  std::cout << "Loading " << filename << std::endl;

  std::ifstream ifs(filename, std::ios::binary);

  if (!ifs) {
    std::cerr << "Failed to open " << filename << std::endl;
    return 1;
  }
  laspp::LASReader reader(ifs);
  std::cout << reader.header() << std::endl;

  auto vlrs = reader.vlr_headers();
  for (const auto& vlr : vlrs) {
    std::cout << vlr << std::endl;
  }

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

  auto evlrs = reader.evlr_headers();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  std::vector<LASPoint> points(reader.num_points());

  reader.read_chunks<LASPoint>(points, {0, reader.num_chunks()});
  std::cout << points[points.size() - 1] << std::endl;

  return 0;
}
