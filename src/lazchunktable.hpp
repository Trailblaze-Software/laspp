#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

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
  std::vector<uint32_t> number_of_points;

 public:
  explicit LAZChunkTable(std::istream& istream) {
    istream.read(reinterpret_cast<char*>(this), sizeof(LAZChunkTableHeader));
  }

  friend std::ostream& operator<<(std::ostream& os, const LAZChunkTable& chunk_table) {
    os << static_cast<const LAZChunkTableHeader>(chunk_table) << std::endl;
    return os;
  }
};

#pragma pack(pop)

}  // namespace laspp
