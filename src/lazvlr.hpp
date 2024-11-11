#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace laspp {

#pragma pack(push, 1)

enum class LAZCompressor : uint16_t {
  None = 0,
  Pointwise = 1,
  PointwiseChunked = 2,
  LayeredChunked = 3,
};

struct __attribute__((packed)) LAZSpecialVLRPt1 {
  LAZCompressor compressor;
  uint16_t coder;  // 0 for arithmetic coder
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t version_revision;
  uint32_t compatibility_mode : 1;
  uint32_t options_reserved : 31;
  uint32_t chunk_size;
  int64_t num_special_evlrs;        // reserved, -1
  int64_t offset_of_special_evlrs;  // reserved, -1
  uint16_t num_item_records;

  bool adaptive_chunking() const { return chunk_size == std::numeric_limits<uint32_t>::max(); }
};

enum class LAZItemType : uint16_t {
  Byte = 0,
  Short = 1,    // unsupported
  Integer = 2,  // unsupported
  Long = 3,     // unsupported
  Float = 4,    // unsupported
  Double = 5,   // unsupported
  Point10 = 6,
  GPSTime11 = 7,
  RGB12 = 8,
  Wavepacket13 = 9,
  Point14 = 10,
  RGB14 = 11,
  RGBNIR14 = 12,
  Wavepacket14 = 13,
  Byte14 = 14,
};

inline bool check_size_from_type(LAZItemType type, uint16_t size) {
  if (size == 0) return false;
  switch (type) {
    case LAZItemType::Byte:
      return true;
    case LAZItemType::Short:
      return size % 2 == 0;
    case LAZItemType::Integer:
      return size % 4 == 0;
    case LAZItemType::Long:
      return size % 8 == 0;
    case LAZItemType::Float:
      return size % 4 == 0;  // Spec says 8??
    case LAZItemType::Double:
      return size % 8 == 0;
    case LAZItemType::Point10:
      return size == 20;
    case LAZItemType::GPSTime11:
      return size == 8;
    case LAZItemType::RGB12:
      return size == 6;
    case LAZItemType::Wavepacket13:
      return size == 29;
    case LAZItemType::Point14:
      return size == 30;
    case LAZItemType::RGB14:
      return size == 6;
    case LAZItemType::RGBNIR14:
      return size == 8;
    case LAZItemType::Wavepacket14:
      return size == 29;
    case LAZItemType::Byte14:
      return true;
  }
}

struct __attribute__((packed)) LAZItemRecord {
  LAZItemType item_type;
  uint16_t item_size;
  uint16_t item_version;
};

struct LAZSpecialVLR : LAZSpecialVLRPt1 {
  std::vector<LAZItemRecord> items_records;

  LAZSpecialVLR(LAZSpecialVLRPt1 pt1)
      : LAZSpecialVLRPt1(pt1), items_records(pt1.num_item_records) {}
};

#pragma pack(pop)

}  // namespace laspp
