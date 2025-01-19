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
#include <vector>

#include "utilities/macros.hpp"

namespace laspp {

#pragma pack(push, 1)

enum class TIFFTagLocation : uint16_t {
  UnsignedShort = 0,
  GeoDoubleParams = 34736,
  GeoAsciiParams = 34737,
};

inline std::ostream& operator<<(std::ostream& os, const TIFFTagLocation& tag_location) {
  switch (tag_location) {
    case TIFFTagLocation::UnsignedShort:
      os << "Unsigned Short";
      break;
    case TIFFTagLocation::GeoDoubleParams:
      os << "Geo Double Params";
      break;
    case TIFFTagLocation::GeoAsciiParams:
      os << "Geo ASCII Params";
      break;
    default:
      os << "Unknown " << static_cast<uint16_t>(tag_location);
      break;
  }
  return os;
}

enum class ExtraBytesDataType : uint8_t {
  Undocumented = 0,
  UnsignedChar = 1,
  Char = 2,
  UnsignedShort = 3,
  Short = 4,
  UnsignedLong = 5,
  Long = 6,
  UnsignedLongLong = 7,
  LongLong = 8,
  Float = 9,
  Double = 10,
};

struct LASPP_PACKED sGeoKeys {
  uint16_t wKeyDirectoryVersion;
  uint16_t wKeyRevision;
  uint16_t wMinorRevision;
  uint16_t wNumberOfKeys;

  struct LASPP_PACKED sKeyEntry {
    uint16_t wKeyID;
    TIFFTagLocation wTIFFTagLocation;
    uint16_t wCount;
    uint16_t wValue_Offset;
  };
};

struct GeoKeys : sGeoKeys {
  std::vector<sKeyEntry> keys;

  GeoKeys(std::istream& in_stream) {
    in_stream.read(reinterpret_cast<char*>(this), sizeof(sGeoKeys));
    keys.resize(wNumberOfKeys);
    in_stream.read(reinterpret_cast<char*>(keys.data()), sizeof(sKeyEntry) * wNumberOfKeys);
  }

  friend std::ostream& operator<<(std::ostream& os, const GeoKeys& geo_keys) {
    os << "Key Directory Version: " << geo_keys.wKeyDirectoryVersion << std::endl;
    os << "Key Revision: " << geo_keys.wKeyRevision << std::endl;
    os << "Minor Revision: " << geo_keys.wMinorRevision << std::endl;
    os << "Number of Keys: " << geo_keys.wNumberOfKeys << std::endl;
    for (const auto& key : geo_keys.keys) {
      os << "Key ID: " << key.wKeyID << std::endl;
      os << "TIFF Tag Location: " << key.wTIFFTagLocation << std::endl;
      os << "Count: " << key.wCount << std::endl;
      os << "Value Offset: " << key.wValue_Offset << std::endl;
    }
    return os;
  }
};

struct LASPP_PACKED ExtraBytesInfo {
  uint8_t reserved[2];
  uint8_t data_type;
  uint8_t no_data_bit : 1;
  uint8_t min_bit : 1;
  uint8_t max_bit : 1;
  uint8_t scale_bit : 1;
  uint8_t offset_bit : 1;
  char name[32];
  uint8_t unused[4];
  int64_t no_data;
  uint8_t deprecated1[16];
  int64_t min;
  uint8_t deprecated2[16];
  int64_t max;
  uint8_t deprecated3[16];
  double scale;
  uint8_t deprecated4[16];
  double offset;
  uint8_t deprecated5[16];
  char description[32];
};

struct LASPP_PACKED WaveformPacketDescriptor {
  uint8_t bits_per_sample;   // (2-32)
  uint8_t compression_type;  // 0 (compression unsupported)
  uint32_t number_of_samples;
  uint32_t temporal_spacing;  // picoseconds
  double digitizer_gain;
  double digitizer_offset;

  double to_digitizer_units(double value) const {
    return value * digitizer_gain + digitizer_offset;
  }
};

struct LASPP_PACKED LASVariableLengthRecord {
  uint16_t reserved;
  char user_id[16];
  uint16_t record_id;
  uint16_t record_length_after_header;
  char description[32];

  friend std::ostream& operator<<(std::ostream& os, const LASVariableLengthRecord& vlr) {
    os << "Reserved: " << vlr.reserved << std::endl;
    os << "User ID: " << vlr.user_id << std::endl;
    os << "Record ID: " << vlr.record_id << std::endl;
    os << "Record length after header: " << vlr.record_length_after_header << std::endl;
    os << "Description: " << vlr.description << std::endl;
    return os;
  }

  bool is_projection() const { return std::string(user_id) == "LASF_Projection"; }

  bool is_ogc_math_transform_wkt() const { return is_projection() && record_id == 2111; }

  bool is_ogc_coordinate_system_wkt() const { return is_projection() && record_id == 2112; }

  bool is_geo_key_directory() const { return is_projection() && record_id == 34735; }

  bool is_geo_double_params() const { return is_projection() && record_id == 34736; }

  bool is_geo_ascii_params() const { return is_projection() && record_id == 34737; }

  bool is_spec() const { return std::string(user_id) == "LASF_Spec"; }

  bool is_classification_lookup() const { return is_spec() && record_id == 0; }

  bool is_text_area_description() const { return is_spec() && record_id == 3; }

  bool is_extra_bytes_info() const {
    if (is_spec() && record_id == 4) {
      assert(record_length_after_header % sizeof(ExtraBytesInfo) == 0);
      return true;
    }
    return false;
  }

  bool is_superseded() const { return is_spec() && record_id == 7; }

  bool is_waveform_packet_descriptor() const {
    return is_spec() && record_id > 99 && record_id < 355;
  }

  bool is_waveform_data_packets() const { return is_spec() && record_id == 65535; }

  bool is_laz_vlr() const {
    return (reserved == 0 || reserved == 0xAABB) &&  // 43707
           (std::string(user_id) == "LAZ encoded" || std::string(user_id) == "laszip encoded") &&
           record_id == 22204;
  }
};

using LASVLR = LASVariableLengthRecord;

class LASVLRWithGlobalOffset : public LASVariableLengthRecord {
  uint64_t m_global_offset;

 public:
  LASVLRWithGlobalOffset(const LASVariableLengthRecord& vlr, uint64_t global_offset)
      : LASVariableLengthRecord(vlr), m_global_offset(global_offset) {}

  uint64_t global_offset() const { return m_global_offset; }

  using record_type = LASVariableLengthRecord;
};

struct LASPP_PACKED LASExtendedVariableLengthRecord {
  uint16_t reserved;
  char user_id[16];
  uint16_t record_id;
  uint64_t record_length_after_header;
  char description[32];

  friend std::ostream& operator<<(std::ostream& os, const LASExtendedVariableLengthRecord& evlr) {
    os << "Reserved: " << evlr.reserved << std::endl;
    os << "User ID: " << evlr.user_id << std::endl;
    os << "Record ID: " << evlr.record_id << std::endl;
    os << "Record length after header: " << evlr.record_length_after_header << std::endl;
    os << "Description: " << evlr.description << std::endl;
    return os;
  }
};

using LASEVLR = LASExtendedVariableLengthRecord;

class LASEVLRWithGlobalOffset : public LASExtendedVariableLengthRecord {
  uint64_t m_global_offset;

 public:
  LASEVLRWithGlobalOffset(const LASExtendedVariableLengthRecord& evlr, uint64_t global_offset)
      : LASExtendedVariableLengthRecord(evlr), m_global_offset(global_offset) {}

  uint64_t global_offset() const { return m_global_offset; }

  using record_type = LASExtendedVariableLengthRecord;
};

#pragma pack(pop)

}  // namespace laspp
