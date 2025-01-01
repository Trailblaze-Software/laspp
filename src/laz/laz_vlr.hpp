/*
 * SPDX-FileCopyrightText: (c) 2025 Trailblaze Software, all rights reserved
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
  uint16_t coder = 0;  // 0 for arithmetic coder
  uint8_t version_major = 1;
  uint8_t version_minor = 4;
  uint16_t version_revision = 0;
  uint32_t compatibility_mode : 1;  // Store point formats 6-10 as 0-5 plus extra bytes
  uint32_t options_reserved : 31;
  uint32_t chunk_size = 0;
  int64_t num_special_evlrs = -1;        // reserved, -1
  int64_t offset_of_special_evlrs = -1;  // reserved, -1
  uint16_t num_item_records = 0;

  bool adaptive_chunking() const { return chunk_size == std::numeric_limits<uint32_t>::max(); }

  explicit LAZSpecialVLRPt1(std::istream& is) {
    LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(this), sizeof(LAZSpecialVLRPt1)));
  }

  explicit LAZSpecialVLRPt1(LAZCompressor laz_compressor)
      : compressor(laz_compressor), compatibility_mode(false), options_reserved(0) {}

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

inline uint16_t default_size(LAZItemType type) {
  switch (type) {
    case LAZItemType::Byte:
      return 1;
    case LAZItemType::Short:
      return 2;
    case LAZItemType::Integer:
      return 4;
    case LAZItemType::Long:
      return 8;
    case LAZItemType::Float:
      return 4;
    case LAZItemType::Double:
      return 8;
    case LAZItemType::Point10:
      return 20;
    case LAZItemType::GPSTime11:
      return 8;
    case LAZItemType::RGB12:
      return 6;
    case LAZItemType::Wavepacket13:
      return 29;
    case LAZItemType::Point14:
      return 30;
    case LAZItemType::RGB14:
      return 6;
    case LAZItemType::RGBNIR14:
      return 8;
    case LAZItemType::Wavepacket14:
      return 29;
    case LAZItemType::Byte14:
      return 1;
  }
  LASPP_FAIL("Unknown LAZ item type: ", static_cast<uint16_t>(type));
}

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
    case LAZItemType::Byte14:
      return true;
    default:
      return size == default_size(type);
  }
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

  explicit LAZItemRecord(LAZItemType laz_item_type)
      : item_type(laz_item_type),
        item_size(default_size(laz_item_type)),
        item_version(LAZItemVersion::Version1) {}

  LAZItemRecord(LAZItemType laz_item_type, uint16_t laz_item_size)
      : item_type(laz_item_type), item_size(laz_item_size), item_version(LAZItemVersion::Version1) {
    LASPP_ASSERT(check_size_from_type(laz_item_type, laz_item_size));
  }

  LAZItemRecord() = default;

  friend std::ostream& operator<<(std::ostream& os, const LAZItemRecord& record) {
    os << "Item type: " << record.item_type << std::endl;
    os << "Item size: " << record.item_size << std::endl;
    os << "Item version: " << record.item_version << std::endl;
    return os;
  }
};

#pragma pack(pop)

struct LAZSpecialVLR : LAZSpecialVLRPt1 {
  std::vector<LAZItemRecord> items_records;

  explicit LAZSpecialVLR(std::istream& is) : LAZSpecialVLRPt1(is), items_records(num_item_records) {
    for (auto& item : items_records) {
      LASPP_CHECK_READ(is.read(reinterpret_cast<char*>(&item), sizeof(LAZItemRecord)));
      LASPP_ASSERT(check_size_from_type(item.item_type, item.item_size));
    }
  }

  explicit LAZSpecialVLR(LAZCompressor laz_compressor) : LAZSpecialVLRPt1(laz_compressor) {}

  void write_to(std::ostream& os) const {
    os.write(reinterpret_cast<const char*>(this), sizeof(LAZSpecialVLRPt1));
    for (const auto& item : items_records) {
      os.write(reinterpret_cast<const char*>(&item), sizeof(LAZItemRecord));
    }
  }

  void add_item_record(LAZItemRecord item) {
    items_records.push_back(item);
    num_item_records = static_cast<uint16_t>(items_records.size());
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

}  // namespace laspp
