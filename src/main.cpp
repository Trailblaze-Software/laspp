#include <fstream>
#include <iostream>

#include "lasreader.hpp"

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
  }

  auto evlrs = reader.read_evlrs();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  return 0;
}
