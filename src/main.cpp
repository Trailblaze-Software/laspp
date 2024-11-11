#include <cstring>
#include <fstream>
#include <iostream>

#include "lasreader.hpp"
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
      std::cout << laz_vlr_pt1 << std::endl;

      laspp::LAZSpecialVLR laz_vlr(laz_vlr_pt1, ifs);
      std::cout << laz_vlr << std::endl;
    }
  }

  auto evlrs = reader.read_evlrs();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  return 0;
}
