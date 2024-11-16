#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "lasreader.hpp"
#include "laz/bit_symbol_encoder.hpp"
#include "laz/lazchunktable.hpp"
#include "laz/lazvlr.hpp"
#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

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

  // std::cout << reader.get_first_point() << std::endl;

  auto vlrs = reader.read_vlrs();
  for (const auto& vlr : vlrs) {
    std::cout << vlr << std::endl;
    if (vlr.is_laz_vlr()) {
      std::cout << "Found LAZ VLR" << std::endl;
      ifs.seekg(vlr.global_offset());
      laspp::LAZSpecialVLRPt1 laz_vlr_pt1;
      ifs.read(reinterpret_cast<char*>(&laz_vlr_pt1), sizeof(laspp::LAZSpecialVLRPt1));
      laspp::LAZSpecialVLR laz_vlr(laz_vlr_pt1, ifs);
      std::cout << laz_vlr << std::endl;

      ifs.seekg(laz_vlr.offset_of_special_evlrs);
      for (int i = 0; i < laz_vlr.num_special_evlrs; i++) {
        laspp::LASEVLR laz_evlr;
        ifs.read(reinterpret_cast<char*>(&laz_evlr), sizeof(laspp::LASEVLR));
        std::cout << laz_evlr << std::endl;
      }
    }
  }

  ifs.seekg(reader.header().offset_to_point_data());
  int64_t chunk_table_offset;
  // if (chunk_table_offset == -1) use last 8 bytes of file
  ifs.read(reinterpret_cast<char*>(&chunk_table_offset), sizeof(size_t));
  std::cout << "Chunk table offset: " << chunk_table_offset << std::endl;

  ifs.seekg(chunk_table_offset);
  laspp::LAZChunkTable chunk_table(ifs);
  std::cout << "Chunk table:\n" << chunk_table;

  {
    std::stringstream encoded_stream;
    {
      laspp::OutStream ostream(encoded_stream);
      laspp::SymbolEncoder<3> symbol_encoder;
      symbol_encoder.encode_symbol(ostream, 0);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 2);
      symbol_encoder.encode_symbol(ostream, 1);
      symbol_encoder.encode_symbol(ostream, 0);
      symbol_encoder.encode_symbol(ostream, 2);
    }

    std::cout << "Encoded stream:";
    for (char c : encoded_stream.str()) {
      std::cout << " " << (uint32_t)c;
    }
    std::cout << std::endl;

    {
      laspp::InStream instream(encoded_stream);
      laspp::SymbolEncoder<3> symbol_encoder;
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
      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 1);
      symbol_encoder.encode_bit(ostream, 0);
      symbol_encoder.encode_bit(ostream, 1);
    }

    std::cout << "Encoded stream:";
    for (char c : encoded_stream.str()) {
      std::cout << " " << (uint32_t)c;
    }
    std::cout << std::endl;

    {
      laspp::InStream instream(encoded_stream);
      laspp::BitSymbolEncoder symbol_encoder;
      for (size_t i = 0; i < 6; i++) {
        uint32_t bit = symbol_encoder.decode_bit(instream);
        std::cout << "Symbol: " << bit << std::endl;
        ;
      }
    }
  }

  auto evlrs = reader.read_evlrs();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  return 0;
}
