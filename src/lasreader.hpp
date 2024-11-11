#pragma once

#include <vector>

#include "lasheader.hpp"
#include "laspoint.hpp"

namespace laspp {

class LASReader {
  std::ifstream& m_ifs;
  LASHeader m_header;

  LASHeader read_header() {
    m_ifs.seekg(0);
    LASHeader header;
    m_ifs.read(reinterpret_cast<char*>(&header), sizeof(LASHeader));
    return header;
  }

 public:
  LASReader(std::ifstream& ifs) : m_ifs(ifs), m_header(read_header()) {}

  const LASHeader& header() const { return m_header; }

  std::vector<std::byte> read_point_data(const LASHeader& header) {
    std::vector<std::byte> point_data(header.point_data_record_length() * header.num_points());
    m_ifs.seekg(header.offset_to_point_data());
    m_ifs.read(reinterpret_cast<char*>(point_data.data()), point_data.size());
    return point_data;
  }

  LASPointFormat0 get_first_point() {
    std::vector<std::byte> point_data = read_point_data(m_header);
    return *reinterpret_cast<LASPointFormat0*>(point_data.data());
  }
};

}  // namespace laspp
