#pragma once

#include <cstdint>
#include <ios>
#include <limits>
#include <span>

#include "laspoint.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/stream.hpp"
#include "lazchunktable.hpp"
#include "lazvlr.hpp"
#include "utilities/printing.hpp"

namespace laspp {

class PointerStreamBuffer : public std::streambuf {
 public:
  PointerStreamBuffer(std::byte* data, size_t size) {
    setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data),
         reinterpret_cast<char*>(data + size));
  }
};

class LAZReader {
  LAZSpecialVLR m_special_vlr;

 public:  // make private
  std::optional<LAZChunkTable> m_chunk_table;

 public:
  explicit LAZReader(const LAZSpecialVLR& special_vlr) : m_special_vlr(special_vlr) {}

  std::optional<size_t> chunk_size() const {
    if (m_special_vlr.chunk_size == std::numeric_limits<uint32_t>::max()) {
      return std::nullopt;
    }
    return m_special_vlr.chunk_size;
  }

  template <typename T>
  std::span<T> decompress_chunk(std::span<std::byte> compressed_data,
                                std::span<T> decompressed_data) {
    LASPointFormat0 last_las_point = *reinterpret_cast<LASPointFormat0*>(compressed_data.data());
    GPSTime last_gps_time =
        *reinterpret_cast<GPSTime*>(compressed_data.data() + sizeof(LASPointFormat0));
    std::cout << "Decompressed point: " << last_las_point << std::endl;
    decompressed_data[0] = last_las_point;

    std::byte* data_ptr = compressed_data.data() + sizeof(last_las_point) + sizeof(last_gps_time);

    std::vector<std::byte> last_bytes(5);
    for (int i = 0; i < 5; i++) {
      last_bytes[i] = *(data_ptr++);
    }
    std::cout << "Bytes: " << last_bytes << std::endl;

    std::cout << m_special_vlr << std::endl;

    LASPointFormat0Encoder las_point_decompressor(last_las_point);

    PointerStreamBuffer compressed_buffer(
        data_ptr, compressed_data.size() - sizeof(last_las_point) - sizeof(last_gps_time));
    std::istream compressed_stream(&compressed_buffer);
    InStream compressed_in_stream(compressed_stream);
    for (size_t i = 1; i < decompressed_data.size(); i++) {
      LASPointFormat0 next_las_point = las_point_decompressor.decompress(compressed_in_stream);
      std::cout << next_las_point << std::endl;
      decompressed_data[i] = next_las_point;
    }

    return decompressed_data;
  }
};

}  // namespace laspp
