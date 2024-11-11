#pragma once

#include <vector>

#include "lasheader.hpp"
#include "laspoint.hpp"
#include "lasvlr.hpp"

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

  std::vector<LASVLRWithGlobalOffset> read_vlrs() {
    std::vector<LASVLRWithGlobalOffset> vlrs;
    m_ifs.seekg(m_header.VLR_offset());
    for (unsigned int i = 0; i < m_header.VLR_count(); ++i) {
      LASVLR vlr;
      m_ifs.read(reinterpret_cast<char*>(&vlr), sizeof(LASVLR));
      vlrs.push_back(LASVLRWithGlobalOffset(vlr, m_ifs.tellg()));
      m_ifs.seekg(vlr.record_length_after_header, std::ios_base::cur);
    }
    return vlrs;
  }

  std::vector<LASEVLRWithGlobalOffset> read_evlrs() {
    std::vector<LASEVLRWithGlobalOffset> evlrs;
    for (unsigned int i = 0; i < m_header.EVLR_count(); ++i) {
      LASEVLR evlr;
      m_ifs.read(reinterpret_cast<char*>(&evlr), sizeof(LASEVLR));
      evlrs.push_back(LASEVLRWithGlobalOffset(evlr, m_ifs.tellg()));
      m_ifs.seekg(evlr.record_length_after_header, std::ios_base::cur);
    }
    return evlrs;
  }
};

}  // namespace laspp
