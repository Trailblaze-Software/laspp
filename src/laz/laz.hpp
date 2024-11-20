#pragma once

#include <cstdint>
#include <limits>
#include <span>

#include "laspoint.hpp"
#include "lazchunktable.hpp"
#include "lazvlr.hpp"

namespace laspp {

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
    std::cout << "Decompressed point: " << last_las_point << std::endl;
    decompressed_data[0] = last_las_point;
    return decompressed_data.subspan(0, 1);
  }
};

}  // namespace laspp
