#pragma once

#include <span>
#include <type_traits>
#include <vector>

#include "las_header.hpp"
#include "las_point.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

namespace laspp {

enum class WritingStage { VLRS, POINTS, CHUNKTABLE, EVLRS, HEADER };

std::ostream& operator<<(std::ostream& os, WritingStage stage) {
  switch (stage) {
    case WritingStage::VLRS:
      os << "WritingStage::VLRS";
      break;
    case WritingStage::POINTS:
      os << "WritingStage::POINTS";
      break;
    case WritingStage::CHUNKTABLE:
      os << "WritingStage::CHUNKTABLE";
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
  bool m_written_chunktable = false;

  void write_header() {
    m_ofs.seekp(0);
    m_ofs.write(reinterpret_cast<const char*>(&m_header), m_header.size());
  }

 public:
  explicit LASWriter(std::ofstream& ofs, uint8_t point_format, uint16_t num_extra_bytes = 0)
      : m_ofs(ofs) {
    header().set_point_format(point_format, num_extra_bytes);
    header().m_offset_to_point_data = header().size();
    m_ofs.seekp(header().size());
    AssertEQ(num_extra_bytes, 0);
  }

  const LASHeader& header() const { return m_header; }
  LASHeader& header() { return m_header; }

  void write_vlr(const LASVLR& vlr, std::span<const std::byte> data) {
    AssertEQ(m_stage, WritingStage::VLRS);
    AssertEQ(vlr.record_length_after_header, data.size());
    m_ofs.write(reinterpret_cast<const char*>(&vlr), sizeof(LASVLR));
    m_ofs.write(reinterpret_cast<const char*>(data.data()), vlr.record_length_after_header);
    header().m_number_of_variable_length_records++;
    header().m_offset_to_point_data += sizeof(LASVLR) + vlr.record_length_after_header;
  }

 private:
  template <typename PointType, typename T>
  void t_write_points(const std::span<T>& points) {
    AssertLE(m_stage, WritingStage::POINTS);
    m_stage = WritingStage::POINTS;
    AssertEQ(m_header.offset_to_point_data(), m_ofs.tellp());
    m_ofs.seekp(m_header.offset_to_point_data());

    std::vector<PointType> points_to_write(points.size());

    size_t points_by_return[15] = {0};
    int32_t min_pos[3] = {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(),
                          std::numeric_limits<int32_t>::max()};
    int32_t max_pos[3] = {std::numeric_limits<int32_t>::lowest(),
                          std::numeric_limits<int32_t>::lowest(),
                          std::numeric_limits<int32_t>::lowest()};
#pragma omp parallel
    {
      size_t local_points_by_return[15] = {0};
      int32_t local_min_pos[3] = {std::numeric_limits<int32_t>::max(),
                                  std::numeric_limits<int32_t>::max(),
                                  std::numeric_limits<int32_t>::max()};
      int32_t local_max_pos[3] = {std::numeric_limits<int32_t>::lowest(),
                                  std::numeric_limits<int32_t>::lowest(),
                                  std::numeric_limits<int32_t>::lowest()};
#pragma omp for
      for (size_t i = 0; i < points.size(); i++) {
        if constexpr (std::is_base_of_v<LASPointFormat0, PointType> &&
                      is_copy_assignable<LASPointFormat0, T>()) {
          (LASPointFormat0&)points_to_write[i] = static_cast<LASPointFormat0>(points[i]);
          if (points_to_write[i].bit_byte.return_number < 15)
            local_points_by_return[points_to_write[i].bit_byte.return_number]++;
          local_min_pos[0] = std::min(local_min_pos[0], points_to_write[i].x);
          local_min_pos[1] = std::min(local_min_pos[1], points_to_write[i].y);
          local_min_pos[2] = std::min(local_min_pos[2], points_to_write[i].z);
          local_max_pos[0] = std::max(local_max_pos[0], points_to_write[i].x);
          local_max_pos[1] = std::max(local_max_pos[1], points_to_write[i].y);
          local_max_pos[2] = std::max(local_max_pos[2], points_to_write[i].z);
        }
        if constexpr (std::is_base_of_v<GPSTime, PointType> && is_copy_assignable<GPSTime, T>()) {
          (GPSTime&)points_to_write[i] = static_cast<GPSTime>(points[i]);
        }
      }
#pragma omp critical
      {
        for (int i = 0; i < 15; i++) {
          points_by_return[i] += local_points_by_return[i];
        }
        min_pos[0] = std::min(min_pos[0], local_min_pos[0]);
        min_pos[1] = std::min(min_pos[1], local_min_pos[1]);
        min_pos[2] = std::min(min_pos[2], local_min_pos[2]);
        max_pos[0] = std::max(max_pos[0], local_max_pos[0]);
        max_pos[1] = std::max(max_pos[1], local_max_pos[1]);
        max_pos[2] = std::max(max_pos[2], local_max_pos[2]);
      }
    }

    for (int i = 0; i < 15; i++) {
      header().m_number_of_points_by_return[i] += points_by_return[i];
      if (points_by_return[i] > std::numeric_limits<uint32_t>::max()) {
      } else if (i < 5) {
        header().m_legacy_number_of_points_by_return[i] = header().m_number_of_points_by_return[i];
      }
    }

    header().update_bounds({min_pos[0], min_pos[1], min_pos[2]});
    header().update_bounds({max_pos[0], max_pos[1], max_pos[2]});

    header().m_number_of_point_records += points.size();
    if (header().m_number_of_point_records < std::numeric_limits<uint32_t>::max()) {
      header().m_legacy_number_of_point_records = header().m_number_of_point_records;
    }
    std::cout << "Writing " << points_to_write.size() << " points ("
              << sizeof(PointType) * points_to_write.size() << " B)" << std::endl;
    m_ofs.write(reinterpret_cast<const char*>(points_to_write.data()),
                points_to_write.size() * sizeof(PointType));
    AssertEQ(sizeof(PointType), m_header.point_data_record_length());
  }

 public:
  template <typename T>
  void write_points(const std::span<T>& points) {
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      t_write_points<T>(points);
    } else {
      SWITCH_OVER_POINT_TYPE(header().point_format(), t_write_points, points)
    }
  }

  void write_chunktable() {
    if (header().is_laz_compressed() && !m_written_chunktable) {
      AssertLE(m_stage, WritingStage::CHUNKTABLE);
      m_stage = WritingStage::CHUNKTABLE;

      m_written_chunktable = true;
    }
  }

  void write_evlr(const LASEVLR& evlr, const std::vector<std::byte>& data) {
    write_chunktable();
    AssertLE(m_stage, WritingStage::EVLRS);
    m_stage = WritingStage::EVLRS;
    AssertEQ(evlr.record_length_after_header, data.size());
    m_ofs.write(reinterpret_cast<const char*>(&evlr), sizeof(LASEVLR));
    m_ofs.write(reinterpret_cast<const char*>(data.data()), evlr.record_length_after_header);
    header().m_number_of_extended_variable_length_records++;
  }

  ~LASWriter() {
    write_chunktable();
    write_header();
  }
};

}  // namespace laspp
