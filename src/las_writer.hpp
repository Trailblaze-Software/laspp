#pragma once

#include <span>
#include <vector>

#include "las_header.hpp"
#include "vlr.hpp"

namespace laspp {

enum class WritingStage { VLRS, POINTS, EVLRS, HEADER };

std::ostream& operator<<(std::ostream& os, WritingStage stage) {
  switch (stage) {
    case WritingStage::VLRS:
      os << "WritingStage::VLRS";
      break;
    case WritingStage::POINTS:
      os << "WritingStage::POINTS";
      break;
    case WritingStage::EVLRS:
      os << "WritingStage::EVLRS";
      break;
    case WritingStage::HEADER:
      os << "WritingStage::HEADER";
      break;
  }
  return os;
}

class LASWriter {
  std::ofstream& m_ofs;

  LASHeader m_header;

  WritingStage m_stage = WritingStage::VLRS;

  void write_header() {
    m_ofs.seekp(0);
    m_ofs.write(reinterpret_cast<const char*>(&m_header), m_header.size());
  }

 public:
  explicit LASWriter(std::ofstream& ofs, uint8_t point_format, uint16_t num_extra_bytes)
      : m_ofs(ofs) {
    header().set_point_format(point_format, num_extra_bytes);
    m_ofs.seekp(header().size());
  }

  const LASHeader& header() const { return m_header; }
  LASHeader& header() { return m_header; }

  void write_vlr(const LASVLR& vlr, const std::vector<std::byte>& data) {
    AssertEQ(m_stage, WritingStage::VLRS);
    AssertEQ(vlr.record_length_after_header, data.size());
    m_ofs.write(reinterpret_cast<const char*>(&vlr), sizeof(LASVLR));
    m_ofs.write(reinterpret_cast<const char*>(data.data()), vlr.record_length_after_header);
  }

  template <typename T>
  void write_points(const std::span<T>& points) {
    AssertLE(m_stage, WritingStage::POINTS);
    m_stage = WritingStage::POINTS;
    m_ofs.seekp(m_header.offset_to_point_data());
    // m_ofs.write(reinterpret_cast<const char*>(points.data()), points.size_bytes());
    (void)points;
  }

  void write_evlr(const LASEVLR& evlr, const std::vector<std::byte>& data) {
    AssertLE(m_stage, WritingStage::EVLRS);
    m_stage = WritingStage::EVLRS;
    AssertEQ(evlr.record_length_after_header, data.size());
    m_ofs.write(reinterpret_cast<const char*>(&evlr), sizeof(LASEVLR));
    m_ofs.write(reinterpret_cast<const char*>(data.data()), evlr.record_length_after_header);
  }

  ~LASWriter() { write_header(); }
};

}  // namespace laspp
