#include <cstring>
#include <fstream>
#include <iostream>

#include "las_reader.hpp"
#include "las_writer.hpp"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <in_file> <out_file>" << std::endl;
    return 1;
  }

  std::string in_file(argv[1]);
  std::ifstream ifs(in_file, std::ios::binary);
  if (!ifs) {
    std::cerr << "Failed to open " << in_file << std::endl;
    return 1;
  }
  laspp::LASReader reader(ifs);
  std::cout << reader.header() << std::endl;

  auto vlrs = reader.vlr_headers();
  for (const auto& vlr : vlrs) {
    std::cout << vlr << std::endl;
  }

  auto evlrs = reader.evlr_headers();
  for (const auto& evlr : evlrs) {
    std::cout << evlr << std::endl;
  }

  // std::vector<LASPoint> points(reader.num_points());
  //
  // reader.read_chunks<LASPoint>(points, {0, reader.num_chunks()});
  // std::cout << points[points.size() - 1] << std::endl;
  //
  std::string out_file(argv[2]);
  std::ofstream ofs(out_file, std::ios::binary);
  if (!ofs) {
    std::cerr << "Failed to open " << out_file << std::endl;
    return 1;
  }
  {
    laspp::LASWriter writer(ofs, reader.header().point_format(), reader.header().num_extra_bytes());

    writer.header().transform() = reader.header().transform();
  }

  return 0;
}
