#include <cstring>
#include <fstream>
#include <iostream>

#include "lasreader.hpp"
#include "lazchunktable.hpp"
#include "lazvlr.hpp"

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

  auto evlrs = reader.read_evlrs();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  return 0;
}
