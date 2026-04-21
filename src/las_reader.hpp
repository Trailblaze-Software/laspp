/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "example_custom_las_point.hpp"
#include "las_header.hpp"
#include "las_point.hpp"
#include "laz/chunktable.hpp"
#include "laz/laz_reader.hpp"
#include "laz/stream.hpp"
#include "spatial_index.hpp"
#include "utilities/assert.hpp"
#include "utilities/env.hpp"
#include "utilities/memory_mapped_file.hpp"
#include "utilities/thread_pool.hpp"
#include "vlr.hpp"

namespace laspp {

class LASReader {
 public:
  LASReader(const LASReader&) = delete;
  LASReader& operator=(const LASReader&) = delete;
  LASReader(LASReader&&) = delete;
  LASReader& operator=(LASReader&&) = delete;

 private:
  // Support both stream-based and memory-mapped I/O
  std::optional<std::ifstream> m_owned_stream;  // Owned stream (when constructed from path)
  std::istream* m_input_stream = nullptr;       // Pointer to stream (either owned or external)
  std::optional<utilities::MemoryMappedFile> m_mapped_file;  // Memory-mapped file (faster path)
  std::optional<PointerStreamBuffer> m_mapped_buffer;        // Stream buffer for mapped file
  std::optional<std::istream> m_mapped_stream;               // Stream view of mapped file
  std::optional<std::filesystem::path> m_file_path;          // File path for .lax file lookup
  LASHeader m_header;
  std::optional<LAZReader> m_laz_reader;
  std::optional<std::string> m_math_wkt;
  std::optional<std::string> m_coordinate_wkt;
  std::optional<LASGeoKeys> m_las_geo_keys;
  std::optional<QuadtreeSpatialIndex> m_spatial_index;
  std::vector<LASVLRWithGlobalOffset> m_vlr_headers;
  std::vector<LASEVLRWithGlobalOffset> m_evlr_headers;

  // Unified I/O helper: zero-copy view for memory-mapped path, owned buffer for stream path.
  // The returned object is non-copyable; its `data` span is always valid for its lifetime.
  struct ReadBuffer {
    std::vector<std::byte> storage;   // populated only for the stream path
    std::span<const std::byte> data;  // always valid

    explicit ReadBuffer(std::span<const std::byte> span) noexcept : data(span) {}
    explicit ReadBuffer(std::vector<std::byte>&& buf) : storage(std::move(buf)), data(storage) {}

    ReadBuffer(const ReadBuffer&) = delete;
    ReadBuffer& operator=(const ReadBuffer&) = delete;
    ReadBuffer(ReadBuffer&&) = default;
  };

  ReadBuffer get_bytes(size_t offset, size_t size) {
    if (m_mapped_file.has_value()) {
      return ReadBuffer(m_mapped_file->subspan(offset, size));
    }
    LASPP_ASSERT(m_input_stream != nullptr, "m_input_stream must be set for stream-based I/O");

    std::vector<std::byte> buf(size);
    LASPP_CHECK_SEEK(*m_input_stream, offset, std::ios::beg);
    LASPP_CHECK_READ(*m_input_stream, buf.data(), size);

    return ReadBuffer(std::move(buf));
  }

  static LASHeader read_header(std::istream& input_stream) {
    LASPP_CHECK_SEEK(input_stream, 0, std::ios::beg);
    return LASHeader(input_stream);
  }

  template <typename T>
  std::vector<T> read_record_headers(int64_t initial_offset, size_t n_records) {
    std::vector<T> record_headers;
    LASPP_ASSERT(m_input_stream != nullptr, "m_input_stream must be set");
    m_input_stream->seekg(initial_offset);

    std::optional<GeoKeys> geo_keys;
    std::optional<std::vector<double>> geo_doubles;
    std::optional<std::vector<char>> geo_ascii;
    for (unsigned int i = 0; i < n_records; ++i) {
      typename T::record_type record;
      LASPP_CHECK_READ(*m_input_stream, &record, sizeof(typename T::record_type));
      // Calculate absolute file offset
      int64_t absolute_offset = m_input_stream->tellg();
      record_headers.emplace_back(record, static_cast<uint64_t>(absolute_offset));
      int64_t end_of_header_offset = m_input_stream->tellg();
      if constexpr (std::is_same_v<typename T::record_type, LASVLR>) {
        if (record.is_laz_vlr()) {
          LAZSpecialVLRContent laz_vlr(*m_input_stream);
          m_laz_reader.emplace(LAZReader(laz_vlr));
        }
        if (record.is_projection()) {
          if (record.is_ogc_math_transform_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(*m_input_stream, wkt.data(), record.record_length_after_header);
            std::string wkt_string(wkt.begin(), wkt.end());
            LASPP_ASSERT(!m_math_wkt.has_value(), "Multiple math WKTs found in header");
            m_math_wkt.emplace(wkt_string);
          }
          if (record.is_ogc_coordinate_system_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(*m_input_stream, wkt.data(), record.record_length_after_header);
            std::string wkt_string(wkt.data(), wkt.size() - 1);
            LASPP_ASSERT(!m_coordinate_wkt.has_value(), "Multiple coordinate WKTs found in header");
            m_coordinate_wkt.emplace(wkt_string);
          }
          if (record.is_geo_key_directory()) {
            geo_keys.emplace(*m_input_stream);
            LASPP_ASSERT_EQ(m_input_stream->tellg(),
                            end_of_header_offset + record.record_length_after_header);
          }
          if (record.is_geo_double_params()) {
            LASPP_ASSERT_EQ(record.record_length_after_header % sizeof(double), 0);
            geo_doubles.emplace(record.record_length_after_header / sizeof(double));
            LASPP_CHECK_READ(*m_input_stream, geo_doubles->data(),
                             record.record_length_after_header);
          }
          if (record.is_geo_ascii_params()) {
            geo_ascii.emplace(uint64_t(record.record_length_after_header));
            LASPP_CHECK_READ(*m_input_stream, geo_ascii->data(), record.record_length_after_header);
          }
        }
      }
      m_input_stream->seekg(static_cast<int64_t>(record.record_length_after_header) +
                            end_of_header_offset);
      LASPP_ASSERT_NE(m_input_stream->tellg(), -1);
    }

    if (geo_keys.has_value()) {
      m_las_geo_keys.emplace(uint16_t(geo_keys->wKeyDirectoryVersion),
                             uint16_t(geo_keys->wKeyRevision), uint16_t(geo_keys->wMinorRevision));
      for (const auto& key : geo_keys->keys) {
        if (key.wTIFFTagLocation == TIFFTagLocation::UnsignedShort) {
          m_las_geo_keys->add_key(key.wKeyID, key.wValue_Offset);
        } else if (key.wTIFFTagLocation == TIFFTagLocation::GeoDoubleParams) {
          LASPP_ASSERT(geo_doubles.has_value(), "GeoDoubleParams not found");
          m_las_geo_keys->add_key(
              key.wKeyID,
              std::vector<double>(geo_doubles->begin() + key.wValue_Offset,
                                  geo_doubles->begin() + key.wValue_Offset + key.wCount));
        } else {
          LASPP_ASSERT_EQ(key.wTIFFTagLocation, TIFFTagLocation::GeoAsciiParams);
          LASPP_ASSERT(geo_ascii.has_value(), "GeoAsciiParams not found");
          std::string value(geo_ascii->data() + key.wValue_Offset,
                            geo_ascii->data() + key.wValue_Offset + key.wCount);
          m_las_geo_keys->add_key(key.wKeyID, value);
        }
      }
    }

    return record_headers;
  }

  std::vector<LASVLRWithGlobalOffset> read_vlr_headers() {
    return read_record_headers<LASVLRWithGlobalOffset>(m_header.VLR_offset(), m_header.VLR_count());
  }

  std::vector<LASEVLRWithGlobalOffset> read_evlr_headers() {
    auto evlrs = read_record_headers<LASEVLRWithGlobalOffset>(
        static_cast<int64_t>(m_header.EVLR_offset()), m_header.EVLR_count());

    // Check for spatial index EVLR
    for (const auto& evlr : evlrs) {
      if (evlr.is_lastools_spatial_index_evlr()) {
        std::vector<std::byte> data = read_evlr_data(evlr);
        std::stringstream ss;
        ss.write(reinterpret_cast<const char*>(data.data()), static_cast<int64_t>(data.size()));
        ss.seekg(0);  // Rewind to beginning for reading
        LASPP_ASSERT(!m_spatial_index.has_value(), "Multiple spatial index EVLRs found");
        try {
          m_spatial_index.emplace(ss);
        } catch (const std::runtime_error&) {
          // Corrupted or unrecognised EVLR payload: leave m_spatial_index empty so
          // the file can still be opened (mirrors the .lax fallback behaviour).
          // Note: std::ios_base::failure (truncated payload) is also caught here
          // because on libstdc++ it derives from std::system_error → std::runtime_error.
        }
        break;
      }
    }

    // If no spatial index found in EVLRs, try to read from .lax file
    if (!m_spatial_index.has_value() && m_file_path.has_value()) {
      std::filesystem::path lax_path = *m_file_path;
      lax_path.replace_extension(".lax");

      if (std::filesystem::exists(lax_path)) {
        std::ifstream lax_file(lax_path, std::ios::binary);
        if (lax_file.is_open()) {
          try {
            m_spatial_index.emplace(lax_file);
          } catch (const std::runtime_error&) {
            // Corrupted or unrecognised .lax content: leave m_spatial_index empty
            // so the file can still be read without its spatial index.
            // Note: std::ios_base::failure (truncated file) is also caught here
            // because on libstdc++ it derives from std::system_error → std::runtime_error.
          }
        }
      }
    }

    return evlrs;
  }

 public:
  // Constructor from file path - uses memory mapping for optimal performance
  explicit LASReader(const std::filesystem::path& file_path) : m_file_path(file_path) {
    // Allow forcing stream-based I/O via environment variable (for benchmarking/comparison)
    // Any non-empty value other than "0" enables stream-based I/O.
    auto disable_mmap = utilities::get_env("LASPP_DISABLE_MMAP");
    bool force_stream_io =
        disable_mmap.has_value() && !disable_mmap->empty() && (*disable_mmap)[0] != '0';

    if (!force_stream_io) {
      try {
        // Try memory mapping first (fastest)
        m_mapped_file.emplace(file_path.string());
        // Create a stream buffer and stream for reading from mapped memory
        m_mapped_buffer.emplace(m_mapped_file->data());
        m_mapped_stream.emplace(&m_mapped_buffer.value());
        m_input_stream = &m_mapped_stream.value();
        m_header = read_header(*m_input_stream);
      } catch (const std::exception&) {
        // Fallback to stream-based I/O if memory mapping fails
        m_mapped_file.reset();
        m_mapped_buffer.reset();
        m_mapped_stream.reset();
        m_input_stream = nullptr;
      }
    }

    if (!m_mapped_file.has_value()) {
      // Stream-based I/O path (either forced or fallback)
      m_owned_stream.emplace(file_path, std::ios::binary);
      if (!m_owned_stream->is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
      }
      m_input_stream = &m_owned_stream.value();
      m_header = read_header(*m_input_stream);
    }

    m_vlr_headers = read_vlr_headers();
    m_evlr_headers = read_evlr_headers();
    if (m_header.is_laz_compressed()) {
      LASPP_ASSERT(m_laz_reader.has_value(), "LASReader: LAZ point format without LAZ VLR");
      m_input_stream->seekg(header().offset_to_point_data());
      m_laz_reader->read_chunk_table(*m_input_stream, header().num_points());
    }
  }

  // Constructor from istream - uses stream-based I/O (backward compatibility)
  explicit LASReader(std::istream& input_stream,
                     const std::optional<std::filesystem::path>& file_path = std::nullopt)
      : m_input_stream(&input_stream),
        m_file_path(file_path),
        m_header(read_header(*m_input_stream)),
        m_vlr_headers(read_vlr_headers()),
        m_evlr_headers(read_evlr_headers()) {
    if (m_header.is_laz_compressed()) {
      LASPP_ASSERT(m_laz_reader.has_value(), "LASReader: LAZ point format without LAZ VLR");
      m_input_stream->seekg(header().offset_to_point_data());
      m_laz_reader->read_chunk_table(*m_input_stream, header().num_points());
    }
  }

  const LASHeader& header() const { return m_header; }

  const std::vector<LASVLRWithGlobalOffset>& vlr_headers() const { return m_vlr_headers; }
  const std::vector<LASEVLRWithGlobalOffset>& evlr_headers() const { return m_evlr_headers; }

  // Check if memory mapping is being used (for debugging/verification)
  bool is_using_memory_mapping() const noexcept { return m_mapped_file.has_value(); }

  size_t num_points() const { return m_header.num_points(); }
  size_t num_chunks() const {
    if (m_laz_reader.has_value()) {
      return m_laz_reader->chunk_table().num_chunks();
    }
    return 1;
  }
  std::vector<size_t> points_per_chunk() const {
    if (m_laz_reader.has_value()) {
      return m_laz_reader->chunk_table().points_per_chunk();
    }
    return {m_header.num_points()};
  }

 private:
  template <typename CopyType, typename PointType, typename T>
  static void copy_if_possible(const PointType& las_point, T& point) {
    if constexpr (std::is_base_of_v<CopyType, PointType>) {
      if constexpr (is_copy_fromable<T, CopyType>()) {
        copy_from(point, static_cast<const CopyType&>(las_point));
      } else if constexpr (is_copy_assignable<T, CopyType>()) {
        point = static_cast<const CopyType&>(las_point);
      } else if constexpr (std::is_base_of_v<CopyType, T>) {
        static_cast<CopyType&>(point) = static_cast<const CopyType&>(las_point);
      }
    }
  }

  template <typename PointType, typename T>
  void read_points(std::span<T> points) {
    LASPP_ASSERT_EQ(sizeof(PointType), m_header.point_data_record_length());
    size_t point_data_offset = header().offset_to_point_data();
    size_t point_record_length = header().point_data_record_length();

    static_assert(is_copy_assignable<ExampleMinimalLASPoint, LASPointFormat0>());
    static_assert(is_copy_assignable<ExampleFullLASPoint, LASPointFormat0>());
    static_assert(is_copy_fromable<ExampleFullLASPoint, GPSTime>());
    static_assert(is_convertable<T, LASPointFormat0>() || is_convertable<T, LASPointFormat6>() ||
                      is_convertable<T, GPSTime>(),
                  "PointType should use data from LAS file");

    auto buf = get_bytes(point_data_offset, points.size() * point_record_length);
    for (size_t i = 0; i < points.size(); i++) {
      const auto* las_point =
          reinterpret_cast<const PointType*>(buf.data.data() + i * point_record_length);
      copy_if_possible<LASPointFormat0>(*las_point, points[i]);
      copy_if_possible<GPSTime>(*las_point, points[i]);
    }
  }

 public:
  std::optional<std::string> math_wkt() const { return m_math_wkt; }
  std::optional<std::string> coordinate_wkt() const { return m_coordinate_wkt; }
  std::optional<std::string> wkt() const {
    return m_coordinate_wkt.has_value() ? m_coordinate_wkt : m_math_wkt;
  }
  std::optional<LASGeoKeys> geo_keys() const { return m_las_geo_keys; }

  bool has_lastools_spatial_index() const { return m_spatial_index.has_value(); }

  const QuadtreeSpatialIndex& lastools_spatial_index() const {
    LASPP_ASSERT(m_spatial_index.has_value(), "Spatial index not available");
    return *m_spatial_index;
  }

  template <typename T>
  std::span<T> read_chunk(std::span<T> output_location, size_t chunk_index) {
    if (header().is_laz_compressed()) {
      size_t start_offset = m_laz_reader->chunk_table().chunk_offset(chunk_index);
      size_t compressed_chunk_size = m_laz_reader->chunk_table().compressed_chunk_size(chunk_index);
      size_t file_data_offset = header().offset_to_point_data() + start_offset;
      size_t n_points = m_laz_reader->chunk_table().points_per_chunk()[chunk_index];

      auto buf = get_bytes(file_data_offset, compressed_chunk_size);
      return m_laz_reader->decompress_chunk(buf.data, output_location.subspan(0, n_points));
    }
    LASPP_ASSERT(chunk_index == 0);
    size_t n_points = num_points();
    // read_points handles both memory-mapped and stream-based I/O
    if constexpr (std::is_base_of_v<LASPointFormat0, T> || std::is_base_of_v<LASPointFormat6, T>) {
      read_points<T>(output_location);
    } else {
      LASPP_SWITCH_OVER_POINT_TYPE(header().point_format(), read_points, output_location);
    }
    return output_location.subspan(0, n_points);
  }

  template <typename T>
  std::span<T> read_chunks(std::span<T> output_location, std::pair<size_t, size_t> chunk_indexes) {
    if (header().is_laz_compressed()) {
      const auto& chunk_table = m_laz_reader->chunk_table();
      size_t total_n_points =
          (chunk_indexes.second == chunk_table.num_chunks()
               ? header().num_points()
               : chunk_table.decompressed_chunk_offsets()[chunk_indexes.second]) -
          chunk_table.decompressed_chunk_offsets()[chunk_indexes.first];
      LASPP_ASSERT_GE(output_location.size(), total_n_points);

      std::vector<size_t> chunk_indices(chunk_indexes.second - chunk_indexes.first);
      std::iota(chunk_indices.begin(), chunk_indices.end(), chunk_indexes.first);

      if (m_mapped_file.has_value()) {
        // Memory-mapped path: read contiguous block once, then decompress chunks in parallel
        size_t compressed_start_offset = chunk_table.chunk_offset(chunk_indexes.first);
        size_t total_compressed_size = chunk_table.compressed_chunk_size(chunk_indexes.second - 1) +
                                       chunk_table.chunk_offset(chunk_indexes.second - 1) -
                                       compressed_start_offset;
        size_t file_data_offset = header().offset_to_point_data() + compressed_start_offset;
        auto buf = get_bytes(file_data_offset, total_compressed_size);

        utilities::parallel_for(size_t{0}, chunk_indices.size(), [&](size_t idx) {
          size_t chunk_index = chunk_indices[idx];
          size_t start_offset = chunk_table.chunk_offset(chunk_index) - compressed_start_offset;
          size_t compressed_chunk_size = chunk_table.compressed_chunk_size(chunk_index);
          std::span<const std::byte> compressed_chunk =
              buf.data.subspan(start_offset, compressed_chunk_size);

          size_t point_offset = chunk_table.decompressed_chunk_offsets()[chunk_index] -
                                chunk_table.decompressed_chunk_offsets()[chunk_indexes.first];
          size_t n_points = chunk_table.points_per_chunk()[chunk_index];
          if (chunk_index == 0) {
            LASPP_ASSERT_EQ(point_offset, 0u);
          }
          m_laz_reader->decompress_chunk(compressed_chunk,
                                         output_location.subspan(point_offset, n_points));
        });
      } else {
        // Stream-based path: read chunks in parallel (with mutex protection) and decompress.
        // This overlaps I/O and decompression - while one thread decompresses, others can read.
        std::mutex stream_mutex;
        utilities::parallel_for(size_t{0}, chunk_indices.size(), [&](size_t idx) {
          size_t chunk_index = chunk_indices[idx];
          size_t file_data_offset =
              header().offset_to_point_data() + chunk_table.chunk_offset(chunk_index);
          size_t compressed_chunk_size = chunk_table.compressed_chunk_size(chunk_index);

          // Read chunk data with stream mutex protection
          std::vector<std::byte> compressed_buffer;
          {
            std::lock_guard<std::mutex> lock(stream_mutex);
            auto buf = get_bytes(file_data_offset, compressed_chunk_size);
            compressed_buffer = {buf.data.begin(), buf.data.end()};
          }

          // Decompress without holding the lock (allows other threads to read while we decompress)
          size_t point_offset = chunk_table.decompressed_chunk_offsets()[chunk_index] -
                                chunk_table.decompressed_chunk_offsets()[chunk_indexes.first];
          size_t n_points = chunk_table.points_per_chunk()[chunk_index];
          if (chunk_index == 0) {
            LASPP_ASSERT_EQ(point_offset, 0u);
          }
          m_laz_reader->decompress_chunk(std::span<const std::byte>(compressed_buffer),
                                         output_location.subspan(point_offset, n_points));
        });
      }
      return output_location.subspan(0, total_n_points);
    }
    LASPP_ASSERT(chunk_indexes.first == 0);
    LASPP_ASSERT(chunk_indexes.second == 1);
    return read_chunk(output_location, 0);
  }

  std::vector<std::byte> read_vlr_data(const LASVLRWithGlobalOffset& vlr) {
    auto buf = get_bytes(vlr.global_offset(), vlr.record_length_after_header);
    return {buf.data.begin(), buf.data.end()};
  }

  std::vector<std::byte> read_evlr_data(const LASEVLRWithGlobalOffset& evlr) {
    auto buf = get_bytes(evlr.global_offset(), evlr.record_length_after_header);
    return {buf.data.begin(), buf.data.end()};
  }

  // Get chunk indices that contain the given point intervals
  // Returns a sorted list of unique chunk indices.
  // Uses binary search on the monotonically-increasing decompressed_chunk_offsets array
  std::vector<size_t> get_chunk_indices_from_intervals(
      const std::vector<PointInterval>& intervals) const {
    std::vector<size_t> chunk_indices;

    if (header().is_laz_compressed() && m_laz_reader.has_value()) {
      const auto& chunk_table = m_laz_reader->chunk_table();
      const auto& decompressed_offsets = chunk_table.decompressed_chunk_offsets();
      const size_t n_chunks = decompressed_offsets.size();

      if (n_chunks == 0 || intervals.empty()) {
        return chunk_indices;
      }

      for (const auto& interval : intervals) {
        const size_t a = static_cast<size_t>(interval.start);
        const size_t b = static_cast<size_t>(interval.end);

        // First overlapping chunk: the first chunk i where chunk_end[i] >= a.
        // chunk_end[i] = decompressed_offsets[i+1] - 1 (for i < n_chunks-1), so
        // chunk_end[i] >= a  <=>  decompressed_offsets[i+1] > a.
        // Find the first position j in [begin+1, end) where offsets[j] > a;
        // then i = j - 1 is the first chunk whose end reaches a.
        // When j == end (a is past all offset values), first_chunk = n_chunks - 1
        // (the last chunk, whose end extends to total_points - 1).
        auto first_it =
            std::upper_bound(decompressed_offsets.begin() + 1, decompressed_offsets.end(), a);
        size_t first_chunk = static_cast<size_t>(first_it - decompressed_offsets.begin()) - 1;

        // Last overlapping chunk (exclusive upper bound): the first chunk i where
        // chunk_start[i] = decompressed_offsets[i] > b.
        auto last_it =
            std::upper_bound(decompressed_offsets.begin(), decompressed_offsets.end(), b);
        size_t last_chunk_exclusive = static_cast<size_t>(last_it - decompressed_offsets.begin());

        for (size_t i = first_chunk; i < last_chunk_exclusive; ++i) {
          chunk_indices.push_back(i);
        }
      }

      // Remove duplicates and sort
      std::sort(chunk_indices.begin(), chunk_indices.end());
      chunk_indices.erase(std::unique(chunk_indices.begin(), chunk_indices.end()),
                          chunk_indices.end());
    } else {
      // For non-LAZ files, there's only one chunk (index 0)
      if (!intervals.empty()) {
        chunk_indices.push_back(0);
      }
    }

    return chunk_indices;
  }

  // Read a list of chunks (not necessarily contiguous), decompressing in parallel.
  template <typename T>
  std::span<T> read_chunks_list(std::span<T> output_location,
                                const std::vector<size_t>& chunk_indices) {
    if (chunk_indices.empty()) {
      return output_location.subspan(0, 0);
    }

    if (header().is_laz_compressed()) {
      const auto& chunk_table = m_laz_reader->chunk_table();
      const auto& points_per_chunk_vec = chunk_table.points_per_chunk();

      // Pre-compute per-chunk output offsets in a single sequential pass.
      size_t total_points = 0;
      std::vector<size_t> output_offsets(chunk_indices.size());
      for (size_t i = 0; i < chunk_indices.size(); ++i) {
        output_offsets[i] = total_points;
        total_points += points_per_chunk_vec[chunk_indices[i]];
      }

      LASPP_ASSERT_GE(output_location.size(), total_points);

      if (m_mapped_file.has_value()) {
        // Memory-mapped path: get_bytes is zero-copy and thread-safe.
        // Decompress all chunks in parallel — each call uses only local state.
        utilities::parallel_for(size_t{0}, chunk_indices.size(), [&](size_t i) {
          const size_t chunk_idx = chunk_indices[i];
          const size_t file_data_offset =
              header().offset_to_point_data() + chunk_table.chunk_offset(chunk_idx);
          auto buf = get_bytes(file_data_offset, chunk_table.compressed_chunk_size(chunk_idx));
          m_laz_reader->decompress_chunk(
              buf.data,
              output_location.subspan(output_offsets[i], points_per_chunk_vec[chunk_idx]));
        });
      } else {
        // Stream-based path: read chunks in parallel (with mutex protection) and decompress.
        // This overlaps I/O and decompression - while one thread decompresses, others can read.
        std::mutex stream_mutex;
        utilities::parallel_for(size_t{0}, chunk_indices.size(), [&](size_t i) {
          const size_t chunk_idx = chunk_indices[i];
          const size_t file_data_offset =
              header().offset_to_point_data() + chunk_table.chunk_offset(chunk_idx);
          const size_t compressed_size = chunk_table.compressed_chunk_size(chunk_idx);

          // Read chunk data with stream mutex protection
          std::vector<std::byte> compressed_buffer;
          {
            std::lock_guard<std::mutex> lock(stream_mutex);
            auto buf = get_bytes(file_data_offset, compressed_size);
            compressed_buffer = {buf.data.begin(), buf.data.end()};
          }

          // Decompress without holding the lock (allows other threads to read while we decompress)
          m_laz_reader->decompress_chunk(
              std::span<const std::byte>(compressed_buffer),
              output_location.subspan(output_offsets[i], points_per_chunk_vec[chunk_idx]));
        });
      }

      return output_location.subspan(0, total_points);
    } else {
      // For non-LAZ files, there's only one chunk
      LASPP_ASSERT(chunk_indices.size() == 1 && chunk_indices[0] == 0,
                   "Non-LAZ files should only have chunk index 0");
      return read_chunk<T>(output_location, 0);
    }
  }
};

}  // namespace laspp
