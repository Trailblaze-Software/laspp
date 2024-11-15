#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>

#include "lazvlr.hpp"

namespace laspp {

#pragma pack(push, 1)

enum class TIFFTagLocation : unsigned short {
  UnsignedShort = 0,
  GeoDoubleParams = 34736,
  GeoAsciiParams = 34737,
};

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

struct __attribute__((packed)) sGeoKeys {
  unsigned short wKeyDirectoryVersion;
  unsigned short wKeyRevision;
  unsigned short wMinorRevision;
  unsigned short wNumberOfKeys;
  struct sKeyEntry {
    unsigned short wKeyID;
    TIFFTagLocation wTIFFTagLocation;
    unsigned short wCount;
    unsigned short wValue_Offset;
  } pKey[1];
};

struct __attribute__((packed)) ExtraBytesInfo {
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

struct __attribute__((packed)) WaveformPacketDescriptor {
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

struct __attribute__((packed)) LASVariableLengthRecord {
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

  bool geo_double_params() const { return is_projection() && record_id == 34736; }

  bool geo_ascii_params() const { return is_projection() && record_id == 34737; }

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
};

struct __attribute__((packed)) LASExtendedVariableLengthRecord {
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
};

#pragma pack(pop)

}  // namespace laspp
