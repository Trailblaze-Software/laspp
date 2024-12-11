#pragma once

#include "las_header.hpp"

namespace laspp {

class LASWriter {
  std::ofstream& m_ofs;

  LASHeader m_header;

  void write_header() {
    m_ofs.seekp(0);
    m_ofs.write(reinterpret_cast<const char*>(&m_header), m_header.size());
  }

 public:
  explicit LASWriter(std::ofstream& ofs) : m_ofs(ofs) {}

  const LASHeader& header() const { return m_header; }

  ~LASWriter() { write_header(); }
};

}  // namespace laspp
