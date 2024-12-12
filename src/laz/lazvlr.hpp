#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "utilities/assert.hpp"
#include "utilities/macros.hpp"

namespace laspp {

#pragma pack(push, 1)

enum class LAZCompressor : uint16_t {
  None = 0,
  Pointwise = 1,
  PointwiseChunked = 2,
  LayeredChunked = 3,
};

std::ostream& operator<<(std::ostream& os, const LAZCompressor& compressor) {
  switch (compressor) {
    case LAZCompressor::None:
      os << "None";
      break;
    case LAZCompressor::Pointwise:
      os << "Pointwise";
      break;
    case LAZCompressor::PointwiseChunked:
      os << "Pointwise Chunked";
      break;
    case LAZCompressor::LayeredChunked:
      os << "Layered Chunked";
      break;
  }
  return os;
}

struct LASPP_PACKED LAZSpecialVLRPt1 {
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

  friend std::ostream& operator<<(std::ostream& os, const LAZSpecialVLRPt1& pt1) {
    os << "Compressor: " << pt1.compressor << std::endl;
    os << "Coder: " << pt1.coder << std::endl;
    os << "Version: " << static_cast<int>(pt1.version_major) << "."
       << static_cast<int>(pt1.version_minor) << "." << pt1.version_revision << std::endl;
    os << "Compatibility mode: " << pt1.compatibility_mode << std::endl;
    os << "Chunk size: " << pt1.chunk_size << std::endl;
    os << "Adaptive chunking: " << pt1.adaptive_chunking() << std::endl;
    os << "Number of special EVLRs: " << pt1.num_special_evlrs << std::endl;
    os << "Offset of special EVLRs: " << pt1.offset_of_special_evlrs << std::endl;
    os << "Number of item records: " << pt1.num_item_records << std::endl;
    return os;
  }
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
  unreachable();
}

inline std::ostream& operator<<(std::ostream& os, const LAZItemType& type) {
  switch (type) {
    case LAZItemType::Byte:
      os << "Byte";
      break;
    case LAZItemType::Short:
      os << "Short";
      break;
    case LAZItemType::Integer:
      os << "Integer";
      break;
    case LAZItemType::Long:
      os << "Long";
      break;
    case LAZItemType::Float:
      os << "Float";
      break;
    case LAZItemType::Double:
      os << "Double";
      break;
    case LAZItemType::Point10:
      os << "Point 10";
      break;
    case LAZItemType::GPSTime11:
      os << "GPSTime 11";
      break;
    case LAZItemType::RGB12:
      os << "RGB 12";
      break;
    case LAZItemType::Wavepacket13:
      os << "Wavepacket 13";
      break;
    case LAZItemType::Point14:
      os << "Point 14";
      break;
    case LAZItemType::RGB14:
      os << "RGB 14";
      break;
    case LAZItemType::RGBNIR14:
      os << "RGBNIR 14";
      break;
    case LAZItemType::Wavepacket14:
      os << "Wavepacket 14";
      break;
    case LAZItemType::Byte14:
      os << "Byte 14";
      break;
    default:
      os << "Unknown " << static_cast<uint16_t>(type);
      break;
  }
  return os;
}

enum class LAZItemVersion : uint16_t {
  NoCompression = 0,
  Version1 = 1,          // Wavepacket 13
  Version2 = 2,          // Point 10, GPSTime 11, RGB 12, Byte
  ArithmeticCoding = 3,  // Point 14, RGB 14, RGBNIR 14, Wavepacket 14, Byte 14
};

inline std::ostream& operator<<(std::ostream& os, const LAZItemVersion& version) {
  switch (version) {
    case LAZItemVersion::NoCompression:
      os << "No compression";
      break;
    case LAZItemVersion::Version1:
      os << "Version 1";
      break;
    case LAZItemVersion::Version2:
      os << "Version 2";
      break;
    case LAZItemVersion::ArithmeticCoding:
      os << "Arithmetic coding";
      break;
    default:
      os << "Unknown " << static_cast<uint16_t>(version);
      break;
  }
  return os;
}

struct LASPP_PACKED LAZItemRecord {
  LAZItemType item_type;
  uint16_t item_size;
  LAZItemVersion item_version;
};

struct LAZSpecialVLR : LAZSpecialVLRPt1 {
  std::vector<LAZItemRecord> items_records;

  LAZSpecialVLR(const LAZSpecialVLRPt1& pt1, std::istream& is)
      : LAZSpecialVLRPt1(pt1), items_records(pt1.num_item_records) {
    for (auto& item : items_records) {
      is.read(reinterpret_cast<char*>(&item), sizeof(LAZItemRecord));
      if (is.fail()) {
        throw std::runtime_error("LASPP_FAILed to read LAZ item record");
      }
      assert(check_size_from_type(item.item_type, item.item_size));
    }
  }

  friend std::ostream& operator<<(std::ostream& os, const LAZSpecialVLR& vlr) {
    os << static_cast<const LAZSpecialVLRPt1&>(vlr);
    os << "Item records:" << std::endl;
    for (const auto& item : vlr.items_records) {
      os << "  Item type: " << item.item_type << std::endl;
      os << "  Item size: " << item.item_size << std::endl;
      os << "  Item version: " << item.item_version << std::endl;
    }
    return os;
  }
};

#pragma pack(pop)

}  // namespace laspp
