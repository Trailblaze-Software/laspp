
/*
 * Copyright (C) 2024 Trailblaze Software
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For closed source licensing or development requests, contact
 * trailblaze.software@gmail.com
 */

#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ostream>

#include "utilities/macros.hpp"

namespace laspp {

#pragma pack(push, 1)

enum class LASClassification : uint8_t {
  CreatedNeverClassified = 0,
  Unclassified = 1,
  Ground = 2,
  LowVegetation = 3,
  MediumVegetation = 4,
  HighVegetation = 5,
  Building = 6,
  LowPoint = 7,
  ModelKeyPoint = 8,
  Water = 9,
  Rail = 10,
  RoadSurface = 11,
  OverlapPoints = 12,
  WireGuard = 13,
  WireConductor = 14,
  TransmissionTower = 15,
  WireStructureConnector = 16,
  BridgeDeck = 17,
  HighNoise = 18,
  OverheadStructure = 19,
  IgnoredGround = 20,
  Snow = 21,
  TemporalExclusion = 22,
};

inline std::ostream& operator<<(std::ostream& os, const LASClassification& classification) {
  switch (classification) {
    case LASClassification::CreatedNeverClassified:
      os << "Created, never classified";
      break;
    case LASClassification::Unclassified:
      os << "Unclassified";
      break;
    case LASClassification::Ground:
      os << "Ground";
      break;
    case LASClassification::LowVegetation:
      os << "Low vegetation";
      break;
    case LASClassification::MediumVegetation:
      os << "Medium vegetation";
      break;
    case LASClassification::HighVegetation:
      os << "High vegetation";
      break;
    case LASClassification::Building:
      os << "Building";
      break;
    case LASClassification::LowPoint:
      os << "Low point";
      break;
    case LASClassification::ModelKeyPoint:
      os << "Model key point";
      break;
    case LASClassification::Water:
      os << "Water";
      break;
    case LASClassification::Rail:
      os << "Rail";
      break;
    case LASClassification::RoadSurface:
      os << "Road surface";
      break;
    case LASClassification::OverlapPoints:
      os << "Overlap points";
      break;
    case LASClassification::WireGuard:
      os << "Wire guard";
      break;
    case LASClassification::WireConductor:
      os << "Wire conductor";
      break;
    case LASClassification::TransmissionTower:
      os << "Transmission tower";
      break;
    case LASClassification::WireStructureConnector:
      os << "Wire structure connector";
      break;
    case LASClassification::BridgeDeck:
      os << "Bridge deck";
      break;
    case LASClassification::HighNoise:
      os << "High noise";
      break;
    case LASClassification::OverheadStructure:
      os << "Overhead structure";
      break;
    case LASClassification::IgnoredGround:
      os << "Ignored ground";
      break;
    case LASClassification::Snow:
      os << "Snow";
      break;
    case LASClassification::TemporalExclusion:
      os << "Temporal exclusion";
      break;
  }
  return os;
}

struct LASPP_PACKED BitByte {
  uint8_t return_number : 3;
  uint8_t number_of_returns : 3;
  uint8_t scan_direction_flag : 1;
  uint8_t edge_of_flight_line : 1;

  operator uint8_t() { return *reinterpret_cast<uint8_t*>(this); }

  BitByte& operator=(const uint8_t byte) {
    *reinterpret_cast<uint8_t*>(this) = byte;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const BitByte bit_byte) {
    os << "Return number: " << (uint32_t)bit_byte.return_number << std::endl;
    os << "Number of returns: " << (uint32_t)bit_byte.number_of_returns << std::endl;
    os << "Scan direction flag: " << (uint32_t)bit_byte.scan_direction_flag << std::endl;
    os << "Edge of flight line: " << (uint32_t)bit_byte.edge_of_flight_line << std::endl;
    return os;
  }
};

struct LASPP_PACKED ClassificationByte {
  LASClassification classification : 5;
  uint8_t synthetic : 1;
  uint8_t key_point : 1;
  uint8_t withheld : 1;

  operator uint8_t() { return *reinterpret_cast<uint8_t*>(this); }

  ClassificationByte& operator=(const uint8_t byte) {
    *reinterpret_cast<uint8_t*>(this) = byte;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const ClassificationByte& classification_byte) {
    os << "Classification: " << classification_byte.classification << std::endl;
    os << "Synthetic: " << (uint32_t)classification_byte.synthetic << std::endl;
    os << "Key point: " << (uint32_t)classification_byte.key_point << std::endl;
    os << "Withheld: " << (uint32_t)classification_byte.withheld << std::endl;
    return os;
  }
};

struct LASPP_PACKED LASPointFormat0 {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t intensity;
  BitByte bit_byte;
  ClassificationByte classification_byte;
  uint8_t scan_angle_rank;
  uint8_t user_data;
  uint16_t point_source_id;

  LASClassification classification() const { return classification_byte.classification; }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat0& point) {
    os << "X: " << point.x << std::endl;
    os << "Y: " << point.y << std::endl;
    os << "Z: " << point.z << std::endl;
    os << "Intensity: " << point.intensity << std::endl;
    os << point.bit_byte;
    os << point.classification_byte;
    os << "Scan angle rank: " << (uint32_t)point.scan_angle_rank << std::endl;
    os << "User data: " << (uint32_t)point.user_data << std::endl;
    os << "Point source ID: " << point.point_source_id << std::endl;
    return os;
  }
};

typedef union {
  double f64;
  uint64_t uint64;
  int64_t int64;
} DoubleUInt64;

struct LASPP_PACKED GPSTime {
  DoubleUInt64 gps_time;

  uint64_t& as_uint64() { return gps_time.uint64; }

  int64_t& as_int64() { return gps_time.int64; }

  GPSTime(double time = 0) { gps_time.f64 = time; }

  operator double() const { return gps_time.f64; }

  friend std::ostream& operator<<(std::ostream& os, const GPSTime& gpst) {
    return os << "GPS Time: " << std::setprecision(20) << gpst.gps_time.f64 << " ( "
              << *reinterpret_cast<const int64_t*>(&gpst.gps_time) << " )" << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat1 : LASPointFormat0, GPSTime {
  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat1& lpf1) {
    return os << (LASPointFormat0)(lpf1) << (GPSTime)(lpf1) << std::endl;
  }
};

struct LASPP_PACKED ColorData {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
};

struct LASPP_PACKED LASPointFormat2 : LASPointFormat0, ColorData {};

struct LASPP_PACKED LASPointFormat3 : LASPointFormat1, ColorData {};

struct LASPP_PACKED WavePacketData {
  uint8_t wave_packet_descriptor_index;
  uint64_t byte_offset_to_waveform_data;
  uint32_t wave_packet_size;
  float return_point_waveform_location;
  float x_t;
  float y_t;
  float z_t;
};

struct LASPP_PACKED LASPointFormat4 : LASPointFormat1, WavePacketData {};

struct LASPP_PACKED LASPointFormat5 : LASPointFormat3, WavePacketData {};

struct LASPP_PACKED LASPointFormat6 {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t intensity;
  uint8_t return_number : 4;
  uint8_t number_of_returns : 4;
  uint8_t classification_flags : 4;
  uint8_t scanner_channel : 2;
  uint8_t scan_direction_flag : 1;
  uint8_t edge_of_flight_line : 1;
  LASClassification classification;
  uint8_t user_data;
  uint16_t scan_angle;
  uint16_t point_source_id;
  double gps_time;

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat6& point) {
    os << "X: " << point.x << std::endl;
    os << "Y: " << point.y << std::endl;
    os << "Z: " << point.z << std::endl;
    os << "Intensity: " << point.intensity << std::endl;
    os << "Return number: " << (uint32_t)point.return_number << std::endl;
    os << "Number of returns: " << (uint32_t)point.number_of_returns << std::endl;
    os << "Classification flags: " << (uint32_t)point.classification_flags << std::endl;
    os << "Scanner channel: " << (uint32_t)point.scanner_channel << std::endl;
    os << "Scan direction flag: " << (uint32_t)point.scan_direction_flag << std::endl;
    os << "Edge of flight line: " << (uint32_t)point.edge_of_flight_line << std::endl;
    os << "Classification: " << point.classification << std::endl;
    os << "User data: " << (uint32_t)point.user_data << std::endl;
    os << "Scan angle: " << point.scan_angle << std::endl;
    os << "Point source ID: " << point.point_source_id << std::endl;
    return os;
  }
};

struct LASPP_PACKED LASPointFormat7 : LASPointFormat6, ColorData {};

struct LASPP_PACKED LASPointFormat8 : LASPointFormat7 {
  uint16_t NIR;
};

struct LASPP_PACKED LASPointFormat9 : LASPointFormat6, WavePacketData {};

struct LASPP_PACKED LASPointFormat10 : LASPointFormat9, ColorData {
  uint16_t NIR;
};

#define LASPP_SWITCH_OVER_POINT_TYPE_RETURN(format, f, ...) \
  switch (format & (~(1 << 7))) {                           \
    case 0:                                                 \
      return f<laspp::LASPointFormat0>(__VA_ARGS__);        \
    case 1:                                                 \
      return f<laspp::LASPointFormat1>(__VA_ARGS__);        \
    case 2:                                                 \
      return f<laspp::LASPointFormat2>(__VA_ARGS__);        \
    case 3:                                                 \
      return f<laspp::LASPointFormat3>(__VA_ARGS__);        \
    case 4:                                                 \
      return f<laspp::LASPointFormat4>(__VA_ARGS__);        \
    case 5:                                                 \
      return f<laspp::LASPointFormat5>(__VA_ARGS__);        \
    case 6:                                                 \
      return f<laspp::LASPointFormat6>(__VA_ARGS__);        \
    case 7:                                                 \
      return f<laspp::LASPointFormat7>(__VA_ARGS__);        \
    case 8:                                                 \
      return f<laspp::LASPointFormat8>(__VA_ARGS__);        \
    case 9:                                                 \
      return f<laspp::LASPointFormat9>(__VA_ARGS__);        \
    case 10:                                                \
      return f<laspp::LASPointFormat10>(__VA_ARGS__);       \
  }                                                         \
  UNREACHABLE();

#define LASPP_SWITCH_OVER_POINT_TYPE(format, f, ...) \
  switch (format & (~(1 << 7))) {                    \
    case 0:                                          \
      f<laspp::LASPointFormat0>(__VA_ARGS__);        \
      break;                                         \
    case 1:                                          \
      f<laspp::LASPointFormat1>(__VA_ARGS__);        \
      break;                                         \
    case 2:                                          \
      f<laspp::LASPointFormat2>(__VA_ARGS__);        \
      break;                                         \
    case 3:                                          \
      f<laspp::LASPointFormat3>(__VA_ARGS__);        \
      break;                                         \
    case 4:                                          \
      f<laspp::LASPointFormat4>(__VA_ARGS__);        \
      break;                                         \
    case 5:                                          \
      f<laspp::LASPointFormat5>(__VA_ARGS__);        \
      break;                                         \
    case 6:                                          \
      f<laspp::LASPointFormat6>(__VA_ARGS__);        \
      break;                                         \
    case 7:                                          \
      f<laspp::LASPointFormat7>(__VA_ARGS__);        \
      break;                                         \
    case 8:                                          \
      f<laspp::LASPointFormat8>(__VA_ARGS__);        \
      break;                                         \
    case 9:                                          \
      f<laspp::LASPointFormat9>(__VA_ARGS__);        \
      break;                                         \
    case 10:                                         \
      f<laspp::LASPointFormat10>(__VA_ARGS__);       \
      break;                                         \
  }

constexpr std::array<uint16_t, 11> LASPointFormatSize = {
    {sizeof(LASPointFormat0), sizeof(LASPointFormat1), sizeof(LASPointFormat2),
     sizeof(LASPointFormat3), sizeof(LASPointFormat4), sizeof(LASPointFormat5),
     sizeof(LASPointFormat6), sizeof(LASPointFormat7), sizeof(LASPointFormat8),
     sizeof(LASPointFormat9), sizeof(LASPointFormat10)}};

inline uint16_t size_of_point_format(uint8_t format) {
  return LASPointFormatSize[format & (~(1 << 7))];
}

template <typename T1, typename T2, typename = void>
struct CopyAssignable : std::false_type {};

template <typename T1, typename T2>
struct CopyAssignable<T1, T2, decltype(void(std::declval<T1>() = std::declval<T2>()))>
    : std::true_type {};

template <typename T1, typename T2>
using is_copy_assignable = CopyAssignable<T1, T2>;

#pragma pack(pop)

}  // namespace laspp
