/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; version 2.1.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <execution>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <type_traits>
#include <vector>

#include "example_custom_las_point.hpp"
#include "las_header.hpp"
#include "las_point.hpp"
#include "laz/chunktable.hpp"
#include "laz/laz_reader.hpp"
#include "utilities/assert.hpp"
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
  LASHeader m_header;
  std::optional<LAZReader> m_laz_reader;
  std::optional<std::string> m_math_wkt;
  std::optional<std::string> m_coordinate_wkt;
  std::optional<LASGeoKeys> m_las_geo_keys;
  std::vector<LASVLRWithGlobalOffset> m_vlr_headers;
  std::vector<LASEVLRWithGlobalOffset> m_evlr_headers;

  // Helper to get a span from memory-mapped file
  std::span<const std::byte> get_data_span(size_t offset, size_t size) const {
    LASPP_ASSERT(m_mapped_file.has_value(), "get_data_span: not available with stream-based I/O");
    return m_mapped_file->subspan(offset, size);
  }

  static LASHeader read_header(std::istream& input_stream) {
    input_stream.seekg(0);
    return LASHeader(input_stream);
  }

  static LASHeader read_header_from_mapped(const utilities::MemoryMappedFile& mapped_file) {
    // Read header directly from mapped memory
    LASPP_ASSERT_GE(mapped_file.size(), 375u);  // Minimum LAS header size
    std::stringstream header_stream;
    header_stream.write(reinterpret_cast<const char*>(mapped_file.data().data()), 375);
    header_stream.seekg(0);
    return LASHeader(header_stream);
  }

  template <typename T>
  std::vector<T> read_record_headers(int64_t initial_offset, size_t n_records) {
    std::vector<T> record_headers;
    // For VLR reading, create a stream view from mapped memory if available
    // This is less critical (only runs once during construction) but still benefits from mapping
    std::unique_ptr<std::stringstream> mapped_stream;
    std::istream* stream_to_use = m_input_stream;

    if (m_mapped_file.has_value()) {
      // Create stringstream from mapped memory for VLR reading
      auto header_data =
          m_mapped_file->subspan(static_cast<size_t>(initial_offset),
                                 m_mapped_file->size() - static_cast<size_t>(initial_offset));
      mapped_stream = std::make_unique<std::stringstream>(
          std::string(reinterpret_cast<const char*>(header_data.data()), header_data.size()));
      stream_to_use = mapped_stream.get();
    } else {
      m_input_stream->seekg(initial_offset);
    }

    std::optional<GeoKeys> geo_keys;
    std::optional<std::vector<double>> geo_doubles;
    std::optional<std::vector<char>> geo_ascii;
    for (unsigned int i = 0; i < n_records; ++i) {
      typename T::record_type record;
      LASPP_CHECK_READ(stream_to_use->read(reinterpret_cast<char*>(&record),
                                           static_cast<int64_t>(sizeof(typename T::record_type))));
      // Calculate absolute file offset: if using memory mapping, tellg() is relative to subspan
      // start
      int64_t relative_offset = stream_to_use->tellg();
      int64_t absolute_offset =
          m_mapped_file.has_value() ? initial_offset + relative_offset : relative_offset;
      record_headers.emplace_back(record, static_cast<uint64_t>(absolute_offset));
      int64_t end_of_header_offset = stream_to_use->tellg();
      if constexpr (std::is_same_v<typename T::record_type, LASVLR>) {
        if (record.is_laz_vlr()) {
          LAZSpecialVLRContent laz_vlr(*stream_to_use);
          m_laz_reader.emplace(LAZReader(laz_vlr));
        }
        if (record.is_projection()) {
          if (record.is_ogc_math_transform_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(stream_to_use->read(wkt.data(), record.record_length_after_header));
            std::string wkt_string(wkt.begin(), wkt.end());
            LASPP_ASSERT(!m_math_wkt.has_value(), "Multiple math WKTs found in header");
            m_math_wkt.emplace(wkt_string);
          }
          if (record.is_ogc_coordinate_system_wkt()) {
            std::vector<char> wkt(record.record_length_after_header);
            LASPP_CHECK_READ(stream_to_use->read(wkt.data(), record.record_length_after_header));
            std::string wkt_string(wkt.data(), wkt.size() - 1);
            LASPP_ASSERT(!m_coordinate_wkt.has_value(), "Multiple coordinate WKTs found in header");
            m_coordinate_wkt.emplace(wkt_string);
          }
          if (record.is_geo_key_directory()) {
            geo_keys.emplace(*stream_to_use);
            LASPP_ASSERT_EQ(stream_to_use->tellg(),
                            end_of_header_offset + record.record_length_after_header);
          }
          if (record.is_geo_double_params()) {
            LASPP_ASSERT_EQ(record.record_length_after_header % sizeof(double), 0);
            geo_doubles.emplace(record.record_length_after_header / sizeof(double));
            LASPP_CHECK_READ(stream_to_use->read(reinterpret_cast<char*>(geo_doubles->data()),
                                                 record.record_length_after_header));
          }
          if (record.is_geo_ascii_params()) {
            geo_ascii.emplace(uint64_t(record.record_length_after_header));
            LASPP_CHECK_READ(
                stream_to_use->read(geo_ascii->data(), record.record_length_after_header));
          }
        }
      }
      if (!m_mapped_file.has_value()) {
        // Only seek if using stream (mapped file uses stringstream which auto-advances)
        m_input_stream->seekg(static_cast<int64_t>(record.record_length_after_header) +
                              end_of_header_offset);
        LASPP_ASSERT_NE(m_input_stream->tellg(), -1);
      }
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
    return read_record_headers<LASEVLRWithGlobalOffset>(
        static_cast<int64_t>(m_header.EVLR_offset()), m_header.EVLR_count());
  }

 public:
  // Constructor from file path - uses memory mapping for optimal performance
  explicit LASReader(const std::filesystem::path& file_path) {
    // Allow forcing stream-based I/O via environment variable (for benchmarking/comparison)
    // Validate environment variable: only accept "0" or non-empty non-zero string
    const char* disable_mmap_env = nullptr;
#ifdef _MSC_VER
    // Use _dupenv_s on MSVC to avoid deprecated getenv warning treated as error
    char* env_buf = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&env_buf, &env_len, "LASPP_DISABLE_MMAP") == 0 && env_buf != nullptr) {
      // Validate: only use if it's a reasonable length (prevent DoS)
      if (env_len > 0 && env_len < 1024) {
        disable_mmap_env = env_buf;
      } else {
        std::free(env_buf);
        env_buf = nullptr;
      }
    }
#else
    const char* raw_env = std::getenv("LASPP_DISABLE_MMAP");
    if (raw_env != nullptr) {
      size_t env_len = 0;
      const char* p = raw_env;
      bool is_null_terminated = false;
      while (env_len < 1024) {
        if (*p == '\0') {
          is_null_terminated = true;
          break;
        }
        ++env_len;
        ++p;
      }
      if (is_null_terminated && env_len > 0 && env_len < 1024) {
        disable_mmap_env = raw_env;
      }
    }
#endif

    bool force_stream_io =
        (disable_mmap_env != nullptr && disable_mmap_env[0] != '\0' && disable_mmap_env[0] != '0');

    if (!force_stream_io) {
      try {
        // Try memory mapping first (fastest)
        m_mapped_file.emplace(file_path.string());
        m_header = read_header_from_mapped(*m_mapped_file);
        // Create a dummy stream for backward compatibility (won't be used for reads)
        m_owned_stream.emplace(file_path, std::ios::binary);
        m_input_stream = &m_owned_stream.value();
      } catch (const std::exception&) {
        // Fallback to stream-based I/O if memory mapping fails
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

#ifdef _MSC_VER
    if (env_buf) {
      std::free(env_buf);
    }
#endif
    m_vlr_headers = read_vlr_headers();
    m_evlr_headers = read_evlr_headers();
    if (m_header.is_laz_compressed()) {
      LASPP_ASSERT(m_laz_reader.has_value(), "LASReader: LAZ point format without LAZ VLR");
      if (m_mapped_file.has_value()) {
        // Read chunk table from mapped memory
        // read_chunk_table expects: first 8 bytes = offset to chunk table, then seeks there
        size_t chunk_table_start = header().offset_to_point_data();

        // Read the 8-byte absolute offset from the file
        int64_t chunk_table_absolute_offset;
        std::memcpy(&chunk_table_absolute_offset, m_mapped_file->data().data() + chunk_table_start,
                    sizeof(chunk_table_absolute_offset));
        if (chunk_table_absolute_offset == -1) {
          LASPP_UNIMPLEMENTED("Reading chunk table from LAS file");
        }

        // Calculate how much chunk table data we need
        size_t chunk_table_file_offset = static_cast<size_t>(chunk_table_absolute_offset);
        size_t remaining_file_size = m_mapped_file->size() - chunk_table_file_offset;
        size_t chunk_table_buffer_size = std::min(remaining_file_size, size_t{1024 * 1024});

        // Create stringstream that mimics what read_chunk_table expects:
        // First 8 bytes: relative offset (0, since chunk table data follows immediately)
        // Then: the actual chunk table data
        std::stringstream chunk_table_stream(std::ios::binary | std::ios::in | std::ios::out);
        int64_t relative_offset = 8;  // Chunk table data starts 8 bytes into our stream
        chunk_table_stream.write(reinterpret_cast<const char*>(&relative_offset),
                                 sizeof(relative_offset));
        auto chunk_table_data =
            m_mapped_file->subspan(chunk_table_file_offset, chunk_table_buffer_size);
        chunk_table_stream.write(reinterpret_cast<const char*>(chunk_table_data.data()),
                                 static_cast<std::streamsize>(chunk_table_data.size()));
        if (!chunk_table_stream.good()) {
          throw std::runtime_error("Failed to write chunk table data to stream");
        }
        chunk_table_stream.seekg(0);
        m_laz_reader->read_chunk_table(chunk_table_stream, header().num_points());
      } else {
        m_input_stream->seekg(header().offset_to_point_data());
        m_laz_reader->read_chunk_table(*m_input_stream, header().num_points());
      }
    }
  }

  // Constructor from istream - uses stream-based I/O (backward compatibility)
  explicit LASReader(std::istream& input_stream)
      : m_input_stream(&input_stream),
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

    if (m_mapped_file.has_value()) {
      // Fast path: read directly from mapped memory
      for (size_t i = 0; i < points.size(); i++) {
        size_t point_offset = point_data_offset + i * point_record_length;
        const PointType* las_point =
            reinterpret_cast<const PointType*>(m_mapped_file->data().data() + point_offset);
        static_assert(is_copy_assignable<ExampleMinimalLASPoint, LASPointFormat0>());
        static_assert(is_copy_assignable<ExampleFullLASPoint, LASPointFormat0>());
        static_assert(is_copy_fromable<ExampleFullLASPoint, GPSTime>());
        static_assert(is_convertable<T, LASPointFormat0>() ||
                          is_convertable<T, LASPointFormat6>() || is_convertable<T, GPSTime>(),
                      "PointType should use data from LAS file");
        copy_if_possible<LASPointFormat0>(*las_point, points[i]);
        copy_if_possible<GPSTime>(*las_point, points[i]);
      }
    } else {
      // Slow path: read from stream
      m_input_stream->seekg(static_cast<int64_t>(point_data_offset));
      for (size_t i = 0; i < points.size(); i++) {
        PointType las_point;
        LASPP_CHECK_READ(m_input_stream->read(reinterpret_cast<char*>(&las_point),
                                              static_cast<int64_t>(sizeof(PointType))));
        static_assert(is_copy_assignable<ExampleMinimalLASPoint, LASPointFormat0>());
        static_assert(is_copy_assignable<ExampleFullLASPoint, LASPointFormat0>());
        static_assert(is_copy_fromable<ExampleFullLASPoint, GPSTime>());
        static_assert(is_convertable<T, LASPointFormat0>() ||
                          is_convertable<T, LASPointFormat6>() || is_convertable<T, GPSTime>(),
                      "PointType should use data from LAS file");
        copy_if_possible<LASPointFormat0>(las_point, points[i]);
        copy_if_possible<GPSTime>(las_point, points[i]);
        m_input_stream->seekg(static_cast<int64_t>(point_record_length - sizeof(PointType)),
                              std::ios_base::cur);
      }
    }
  }

 public:
  std::optional<std::string> math_wkt() const { return m_math_wkt; }
  std::optional<std::string> coordinate_wkt() const { return m_coordinate_wkt; }
  std::optional<std::string> wkt() const {
    return m_coordinate_wkt.has_value() ? m_coordinate_wkt : m_math_wkt;
  }
  std::optional<LASGeoKeys> geo_keys() const { return m_las_geo_keys; }

  template <typename T>
  std::span<T> read_chunk(std::span<T> output_location, size_t chunk_index) {
    if (header().is_laz_compressed()) {
      size_t start_offset = m_laz_reader->chunk_table().chunk_offset(chunk_index);
      size_t compressed_chunk_size = m_laz_reader->chunk_table().compressed_chunk_size(chunk_index);
      size_t file_data_offset = header().offset_to_point_data() + start_offset;
      size_t n_points = m_laz_reader->chunk_table().points_per_chunk()[chunk_index];

      std::span<std::byte> compressed_data_span;
      std::vector<std::byte> compressed_data_buffer;  // Only allocated if needed

      if (m_mapped_file.has_value()) {
        // Fast path: use span directly from memory-mapped file (no copy!)
        std::span<const std::byte> const_span =
            m_mapped_file->subspan(file_data_offset, compressed_chunk_size);
        // Safe const_cast: decompress_chunk only reads from the data
        compressed_data_span =
            std::span<std::byte>(const_cast<std::byte*>(const_span.data()), const_span.size());
      } else {
        // Slow path: read into buffer from stream
        compressed_data_buffer.resize(compressed_chunk_size);
        m_input_stream->seekg(static_cast<int64_t>(file_data_offset));
        LASPP_CHECK_READ(
            m_input_stream->read(reinterpret_cast<char*>(compressed_data_buffer.data()),
                                 static_cast<int64_t>(compressed_chunk_size)));
        compressed_data_span = std::span<std::byte>(compressed_data_buffer);
      }

      return m_laz_reader->decompress_chunk(compressed_data_span,
                                            output_location.subspan(0, n_points));
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
      size_t compressed_start_offset =
          m_laz_reader->chunk_table().chunk_offset(chunk_indexes.first);
      size_t total_compressed_size =
          m_laz_reader->chunk_table().compressed_chunk_size(chunk_indexes.second - 1) +
          m_laz_reader->chunk_table().chunk_offset(chunk_indexes.second - 1) -
          compressed_start_offset;
      size_t total_n_points =
          (chunk_indexes.second == m_laz_reader->chunk_table().num_chunks()
               ? header().num_points()
               : m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.second]) -
          m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.first];
      LASPP_ASSERT_GE(output_location.size(), total_n_points);

      size_t file_data_offset = header().offset_to_point_data() + compressed_start_offset;
      std::span<std::byte> compressed_data_span;
      std::vector<std::byte> compressed_data_buffer;  // Only allocated for stream-based I/O

      if (m_mapped_file.has_value()) {
        // Fast path: use span directly from memory-mapped file (no copy!)
        std::span<const std::byte> const_span =
            m_mapped_file->subspan(file_data_offset, total_compressed_size);
        // Safe const_cast: decompress_chunk only reads from the data
        compressed_data_span =
            std::span<std::byte>(const_cast<std::byte*>(const_span.data()), const_span.size());
      } else {
        // Slow path: read into buffer from stream
        compressed_data_buffer.resize(total_compressed_size);
        m_input_stream->seekg(static_cast<int64_t>(file_data_offset));
        LASPP_CHECK_READ(
            m_input_stream->read(reinterpret_cast<char*>(compressed_data_buffer.data()),
                                 static_cast<int64_t>(compressed_data_buffer.size())));
        compressed_data_span = std::span<std::byte>(compressed_data_buffer);
      }

      std::vector<size_t> chunk_indices(chunk_indexes.second - chunk_indexes.first);
      std::iota(chunk_indices.begin(), chunk_indices.end(), chunk_indexes.first);

      // Use thread pool for parallel execution (respects LASPP_NUM_THREADS environment variable)
      utilities::parallel_for(size_t{0}, chunk_indices.size(), [&](size_t idx) {
        size_t chunk_index = chunk_indices[idx];
        size_t start_offset =
            m_laz_reader->chunk_table().chunk_offset(chunk_index) - compressed_start_offset;
        size_t compressed_chunk_size =
            m_laz_reader->chunk_table().compressed_chunk_size(chunk_index);
        std::span<std::byte> compressed_chunk =
            compressed_data_span.subspan(start_offset, compressed_chunk_size);

        size_t point_offset =
            m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_index] -
            m_laz_reader->chunk_table().decompressed_chunk_offsets()[chunk_indexes.first];
        size_t n_points = m_laz_reader->chunk_table().points_per_chunk()[chunk_index];
        if (chunk_index == 0) {
          LASPP_ASSERT_EQ(point_offset, 0u);
        }
        m_laz_reader->decompress_chunk(compressed_chunk,
                                       output_location.subspan(point_offset, n_points));
      });
      return output_location.subspan(0, total_n_points);
    }
    LASPP_ASSERT(chunk_indexes.first == 0);
    LASPP_ASSERT(chunk_indexes.second == 1);
    return read_chunk(output_location, 0);
  }

  std::vector<std::byte> read_vlr_data(const LASVLRWithGlobalOffset& vlr) {
    std::vector<std::byte> data(vlr.record_length_after_header);
    if (m_mapped_file.has_value()) {
      auto vlr_span = m_mapped_file->subspan(vlr.global_offset(), vlr.record_length_after_header);
      if (data.size() > 0) {
        std::memcpy(data.data(), vlr_span.data(), data.size());
      }
    } else {
      m_input_stream->seekg(static_cast<int64_t>(vlr.global_offset()));
      LASPP_CHECK_READ(m_input_stream->read(reinterpret_cast<char*>(data.data()),
                                            static_cast<int64_t>(data.size())));
    }
    return data;
  }

  std::vector<std::byte> read_evlr_data(const LASEVLRWithGlobalOffset& evlr) {
    std::vector<std::byte> data(evlr.record_length_after_header);
    if (m_mapped_file.has_value()) {
      auto evlr_span =
          m_mapped_file->subspan(evlr.global_offset(), evlr.record_length_after_header);
      if (data.size() > 0) {
        std::memcpy(data.data(), evlr_span.data(), data.size());
      }
    } else {
      m_input_stream->seekg(static_cast<int64_t>(evlr.global_offset()));
      LASPP_CHECK_READ(m_input_stream->read(reinterpret_cast<char*>(data.data()),
                                            static_cast<int64_t>(data.size())));
    }
    return data;
  }
};

}  // namespace laspp
