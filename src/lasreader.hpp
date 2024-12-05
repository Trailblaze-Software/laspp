#pragma once

#include <optional>
#include <span>
#include <vector>

#include "lasheader.hpp"
#include "laspoint.hpp"
#include "lasvlr.hpp"
#include "laz/laz.hpp"
#include "laz/lazchunktable.hpp"
#include "utilities/assert.hpp"

namespace laspp {

class LASReader {
  std::ifstream& m_ifs;
  LASHeader m_header;
  std::optional<LAZReader> m_laz_data;
  std::vector<LASVLRWithGlobalOffset> m_vlr_headers;
  std::vector<LASEVLRWithGlobalOffset> m_evlr_headers;

  static LASHeader read_header(std::ifstream& ifs) {
    ifs.seekg(0);
    LASHeader las_header;
    ifs.read(reinterpret_cast<char*>(&las_header), sizeof(LASHeader));
    return las_header;
  }

  template <typename T>
  std::vector<T> read_record_headers(size_t initial_offset, size_t n_records) {
    std::vector<T> record_headers;
    m_ifs.seekg(initial_offset);
    for (unsigned int i = 0; i < n_records; ++i) {
      typename T::record_type record;
      m_ifs.read(reinterpret_cast<char*>(&record), sizeof(LASVLR));
      record_headers.emplace_back(record, m_ifs.tellg());
      auto end_of_header_offset = std::ios_base::cur;
      if constexpr (std::is_same_v<typename T::record_type, LASVLR>) {
        if (record.is_laz_vlr()) {
          LAZSpecialVLRPt1 laz_vlr_pt1;
          m_ifs.read(reinterpret_cast<char*>(&laz_vlr_pt1), sizeof(LAZSpecialVLRPt1));
          LAZSpecialVLR laz_vlr(laz_vlr_pt1, m_ifs);
          m_laz_data.emplace(LAZReader(laz_vlr));
        }
      }
      m_ifs.seekg(record.record_length_after_header, end_of_header_offset);
    }
    return record_headers;
  }

  std::vector<LASVLRWithGlobalOffset> read_vlr_headers() {
    return read_record_headers<LASVLRWithGlobalOffset>(m_header.VLR_offset(), m_header.VLR_count());
  }

  std::vector<LASEVLRWithGlobalOffset> read_evlr_headers() {
    return read_record_headers<LASEVLRWithGlobalOffset>(m_header.EVLR_offset(),
                                                        m_header.EVLR_count());
  }

 public:
  explicit LASReader(std::ifstream& ifs)
      : m_ifs(ifs),
        m_header(read_header(m_ifs)),
        m_vlr_headers(read_vlr_headers()),
        m_evlr_headers(read_evlr_headers()) {
    if (m_header.is_laz_compressed()) {
      Assert(m_laz_data.has_value());

      m_ifs.seekg(header().offset_to_point_data());
      int64_t chunk_table_offset;
      // if (chunk_table_offset == -1) use last 8 bytes of file
      ifs.read(reinterpret_cast<char*>(&chunk_table_offset), sizeof(size_t));
      std::cout << "Chunk table offset: " << chunk_table_offset << std::endl;

      m_ifs.seekg(chunk_table_offset);
      m_laz_data->m_chunk_table.emplace(LAZChunkTable(m_ifs, m_laz_data->chunk_size()));
      std::cout << "Chunk table:\n" << m_laz_data->m_chunk_table.value() << std::endl;
    }
  }

  const LASHeader& header() const { return m_header; }

  const std::vector<LASVLRWithGlobalOffset>& vlr_headers() const { return m_vlr_headers; }
  const std::vector<LASEVLRWithGlobalOffset>& evlr_headers() const { return m_evlr_headers; }

  std::vector<std::byte> read_point_data(const LASHeader& header) {
    std::vector<std::byte> point_data(header.point_data_record_length() * header.num_points());
    m_ifs.seekg(header.offset_to_point_data());
    m_ifs.read(reinterpret_cast<char*>(point_data.data()), point_data.size());
    return point_data;
  }

  size_t num_points() const { return m_header.num_points(); }
  size_t num_chunks() const {
    if (m_laz_data.has_value()) {
      Assert(m_laz_data->m_chunk_table.has_value());
      return m_laz_data->m_chunk_table->num_chunks();
    }
    return 1;
  }
  std::vector<size_t> points_per_chunk() const {
    if (m_laz_data.has_value()) {
      Assert(m_laz_data->m_chunk_table.has_value());
      return m_laz_data->m_chunk_table->points_per_chunk();
    }
    return {m_header.num_points()};
  }

  template <typename T>
  std::span<T> read_chunk(std::span<T> output_location, size_t chunk_index) {
    if (header().is_laz_compressed()) {
      size_t start_offset = m_laz_data->m_chunk_table->chunk_offset(chunk_index);
      m_ifs.seekg(header().offset_to_point_data() + start_offset);
      std::vector<std::byte> compressed_data(
          m_laz_data->m_chunk_table->compressed_chunk_size(chunk_index));
      m_ifs.read(reinterpret_cast<char*>(compressed_data.data()), compressed_data.size());
      // size_t n_points = m_laz_data->m_chunk_table->points_per_chunk()[chunk_index];
      size_t n_points = 6;
      return m_laz_data->decompress_chunk(compressed_data, output_location.subspan(0, n_points));
    }
    Assert(chunk_index == 0);
    size_t n_points = 6;
    m_ifs.seekg(header().offset_to_point_data());
    for (size_t i = 0; i < n_points; i++) {
      LASPointFormat1 las_point;
      std::array<std::byte, 5> bytes;
      m_ifs.read(reinterpret_cast<char*>(&las_point), sizeof(las_point));
      m_ifs.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
      std::cout << las_point << std::endl;
      std::cout << "Bytes: " << bytes << std::endl;
      output_location[i] = las_point;
    }
    return output_location.subspan(0, n_points);
  }

  LASPointFormat0 get_first_point() {
    std::vector<std::byte> point_data = read_point_data(m_header);
    return *reinterpret_cast<LASPointFormat0*>(point_data.data());
  }
};

}  // namespace laspp
