#pragma once

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "laz/integer_encoder.hpp"
#include "laz/stream.hpp"
#include "utilities/printing.hpp"

namespace laspp {

#pragma pack(push, 1)

struct __attribute__((packed)) LAZChunkTableHeader {
  uint32_t version;  // 0
  uint32_t number_of_chunks;

  friend std::ostream& operator<<(std::ostream& os, const LAZChunkTableHeader& chunk_header) {
    os << "Version: " << chunk_header.version << std::endl;
    os << "Number of chunks: " << chunk_header.number_of_chunks << std::endl;
    return os;
  }
};

class LAZChunkTable : LAZChunkTableHeader {
  std::vector<uint32_t> compressed_chunk_size;
  std::optional<uint32_t> constant_chunk_size;
  std::vector<uint32_t> number_of_points;

 public:
  explicit LAZChunkTable(std::istream& istream) {
    istream.read(reinterpret_cast<char*>(this), sizeof(LAZChunkTableHeader));

    InStream decoder(istream);
    IntegerEncoder<32> int_decoder;
    for (size_t i = 0; i < number_of_chunks; i++) {
      int32_t decoded_int = int_decoder.decode_int(decoder);
      int32_t previous_int = i == 0 ? 0 : compressed_chunk_size[i - 1];
      compressed_chunk_size.push_back(decoded_int + previous_int);
    }
  }

  friend std::ostream& operator<<(std::ostream& os, const LAZChunkTable& chunk_table) {
    os << static_cast<const LAZChunkTableHeader>(chunk_table) << std::endl;
    os << "Compressed chunk sizes: " << chunk_table.compressed_chunk_size << std::endl;
    if (!chunk_table.constant_chunk_size) {
      os << "# points by chunk: " << chunk_table.number_of_points << std::endl;
    }
    return os;
  }
};

#pragma pack(pop)

}  // namespace laspp
