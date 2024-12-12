#pragma once

#include <optional>
#include <span>
#include <vector>

#include "las_header.hpp"
#include "las_point.hpp"
#include "laz/laz.hpp"
#include "laz/lazchunktable.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

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
      m_laz_data->m_chunk_table.emplace(
          LAZChunkTable(m_ifs, header().num_points(), m_laz_data->chunk_size()));
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

 private:
  template <typename PointType, typename T>
  void read_points(std::span<T> points) {
    for (size_t i = 0; i < points.size(); i++) {
      PointType las_point;
      m_ifs.read(reinterpret_cast<char*>(&las_point), sizeof(PointType));
      if constexpr (is_copy_assignable<T, LASPointFormat0>() &&
                    std::is_base_of_v<LASPointFormat0, PointType>) {
        points[i] = (LASPointFormat0&)las_point;
      } else if constexpr (std::is_base_of_v<LASPointFormat0, T> &&
                           std::is_base_of_v<LASPointFormat0, PointType>) {
        (LASPointFormat0&)points[i] = (LASPointFormat0&)las_point;
      }
      if constexpr (std::is_base_of_v<GPSTime, PointType> && is_copy_assignable<T, GPSTime>()) {
        points[i] = (GPSTime&)las_point;
      }
      m_ifs.read(nullptr, header().point_data_record_length() - sizeof(PointType));
    }
  }

 public:
  template <typename T>
  std::span<T> read_chunk(std::span<T> output_location, size_t chunk_index) {
    if (header().is_laz_compressed()) {
      size_t start_offset = m_laz_data->m_chunk_table->chunk_offset(chunk_index);
      m_ifs.seekg(header().offset_to_point_data() + start_offset);
      std::vector<std::byte> compressed_data(
          m_laz_data->m_chunk_table->compressed_chunk_size(chunk_index));
      m_ifs.read(reinterpret_cast<char*>(compressed_data.data()), compressed_data.size());
      size_t n_points = m_laz_data->m_chunk_table->points_per_chunk()[chunk_index];
      return m_laz_data->decompress_chunk(compressed_data, output_location.subspan(0, n_points));
    }
    Assert(chunk_index == 0);
    size_t n_points = num_points();
    m_ifs.seekg(header().offset_to_point_data());
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      read_points<T>(output_location);
    } else {
      SWITCH_OVER_POINT_TYPE(header().point_format(), read_points, output_location);
    }
    return output_location.subspan(0, n_points);
  }

  template <typename T>
  std::span<T> read_chunks(std::span<T> output_location, std::pair<size_t, size_t> chunk_indexes) {
    if (header().is_laz_compressed()) {
      size_t compressed_start_offset = m_laz_data->m_chunk_table->chunk_offset(chunk_indexes.first);
      size_t total_compressed_size =
          m_laz_data->m_chunk_table->compressed_chunk_size(chunk_indexes.second - 1) +
          m_laz_data->m_chunk_table->chunk_offset(chunk_indexes.second - 1) -
          compressed_start_offset;
      size_t total_n_points =
          (chunk_indexes.second == m_laz_data->m_chunk_table->num_chunks()
               ? header().num_points()
               : m_laz_data->m_chunk_table->decompressed_chunk_offsets()[chunk_indexes.second]) -
          m_laz_data->m_chunk_table->decompressed_chunk_offsets()[chunk_indexes.first];
      AssertGE(output_location.size(), total_n_points);
      std::vector<std::byte> compressed_data(total_compressed_size);
      m_ifs.seekg(header().offset_to_point_data() + compressed_start_offset);
      std::cout << "Reading " << total_compressed_size << " bytes from " << compressed_start_offset
                << " to " << compressed_start_offset + total_compressed_size << std::endl;
      m_ifs.read(reinterpret_cast<char*>(compressed_data.data()), compressed_data.size());

#pragma omp parallel for schedule(dynamic)
      for (size_t chunk_index = chunk_indexes.first; chunk_index < chunk_indexes.second;
           chunk_index++) {
        size_t start_offset =
            m_laz_data->m_chunk_table->chunk_offset(chunk_index) - compressed_start_offset;
        size_t compressed_chunk_size =
            m_laz_data->m_chunk_table->compressed_chunk_size(chunk_index);
        std::span<std::byte> compressed_chunk =
            ((std::span<std::byte>)compressed_data).subspan(start_offset, compressed_chunk_size);

        size_t point_offset =
            m_laz_data->m_chunk_table->decompressed_chunk_offsets()[chunk_index] -
            m_laz_data->m_chunk_table->decompressed_chunk_offsets()[chunk_indexes.first];
        size_t n_points = m_laz_data->m_chunk_table->points_per_chunk()[chunk_index];
        if (chunk_index == 0) {
          AssertEQ(point_offset, 0);
        }
        m_laz_data->decompress_chunk(compressed_chunk,
                                     output_location.subspan(point_offset, n_points));
      }
      return output_location.subspan(0, total_n_points);
    }
    Assert(chunk_indexes.first == 0);
    Assert(chunk_indexes.second == 1);
    return read_chunk(output_location, 0);
  }

  std::vector<std::byte> read_vlr_data(const LASVLRWithGlobalOffset& vlr) {
    std::vector<std::byte> data(vlr.record_length_after_header);
    m_ifs.seekg(vlr.global_offset());
    m_ifs.read(reinterpret_cast<char*>(data.data()), data.size());
    return data;
  }
};

}  // namespace laspp
