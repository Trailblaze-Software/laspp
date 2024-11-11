#include "lasreader.hpp"

#include <iostream>
#include <fstream>

int main (int argc, char *argv[]) {
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

  std::vector<std::byte> point_data = reader.read_point_data(reader.header());

  std::cout << "Read " << point_data.size() << " bytes of point data" << std::endl;

  std::cout << reader.get_first_point() << std::endl;

  return 0;
}
