
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

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <random>
#include <type_traits>

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

  operator uint8_t() const { return *reinterpret_cast<const uint8_t*>(this); }

  BitByte& operator=(const uint8_t byte) {
    *reinterpret_cast<uint8_t*>(this) = byte;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const BitByte bit_byte) {
    os << "Return number: " << static_cast<int>(bit_byte.return_number) << std::endl;
    os << "Number of returns: " << static_cast<int>(bit_byte.number_of_returns) << std::endl;
    os << "Scan direction flag: " << static_cast<int>(bit_byte.scan_direction_flag) << std::endl;
    os << "Edge of flight line: " << static_cast<int>(bit_byte.edge_of_flight_line) << std::endl;
    return os;
  }
};

struct LASPP_PACKED ClassificationByte {
  LASClassification classification : 5;
  uint8_t synthetic : 1;
  uint8_t key_point : 1;
  uint8_t withheld : 1;

  operator uint8_t() const { return *reinterpret_cast<const uint8_t*>(this); }

  ClassificationByte& operator=(const uint8_t byte) {
    *reinterpret_cast<uint8_t*>(this) = byte;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const ClassificationByte& classification_byte) {
    os << "Classification: " << classification_byte.classification << std::endl;
    os << "Synthetic: " << static_cast<int>(classification_byte.synthetic) << std::endl;
    os << "Key point: " << static_cast<int>(classification_byte.key_point) << std::endl;
    os << "Withheld: " << static_cast<int>(classification_byte.withheld) << std::endl;
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

  static LASPointFormat0 RandomData(std::mt19937_64& gen) {
    std::uniform_int_distribution<int32_t> coord_dist(std::numeric_limits<int32_t>::min(),
                                                      std::numeric_limits<int32_t>::max());
    std::uniform_int_distribution<uint16_t> intensity_dist(0, std::numeric_limits<uint16_t>::max());
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    std::uniform_int_distribution<uint16_t> point_source_dist(0,
                                                              std::numeric_limits<uint16_t>::max());

    LASPointFormat0 point;
    point.x = coord_dist(gen);
    point.y = coord_dist(gen);
    point.z = coord_dist(gen);
    point.intensity = intensity_dist(gen);
    point.bit_byte = byte_dist(gen);
    point.classification_byte = byte_dist(gen);
    point.scan_angle_rank = byte_dist(gen);
    point.user_data = byte_dist(gen);
    point.point_source_id = point_source_dist(gen);
    return point;
  }

  LASClassification classification() const { return classification_byte.classification; }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat0& point) {
    os << "X: " << point.x << std::endl;
    os << "Y: " << point.y << std::endl;
    os << "Z: " << point.z << std::endl;
    os << "Intensity: " << point.intensity << std::endl;
    os << point.bit_byte;
    os << point.classification_byte;
    os << "Scan angle rank: " << static_cast<int>(point.scan_angle_rank) << std::endl;
    os << "User data: " << static_cast<int>(point.user_data) << std::endl;
    os << "Point source ID: " << point.point_source_id << std::endl;
    return os;
  }

  bool operator==(const LASPointFormat0& other) const = default;
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

  static GPSTime RandomData(std::mt19937_64& gen) {
    std::uniform_real_distribution<double> gps_dist(-1e6, 1e6);
    GPSTime gpst;
    gpst.gps_time.f64 = gps_dist(gen);
    return gpst;
  }

  explicit GPSTime(double time) { gps_time.f64 = time; }
  GPSTime() = default;

  operator double() const { return gps_time.f64; }

  friend std::ostream& operator<<(std::ostream& os, const GPSTime& gpst) {
    return os << "GPS Time: " << std::setprecision(20) << gpst.gps_time.f64 << " ( "
              << gpst.gps_time.uint64 << " )" << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat1 : LASPointFormat0, GPSTime {
  bool operator==(const LASPointFormat1& other) const = default;

  static LASPointFormat1 RandomData(std::mt19937_64& gen) {
    LASPointFormat1 point;
    static_cast<LASPointFormat0&>(point) = LASPointFormat0::RandomData(gen);
    static_cast<GPSTime&>(point) = GPSTime::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat1& lpf1) {
    return os << static_cast<const LASPointFormat0&>(lpf1) << static_cast<const GPSTime&>(lpf1)
              << std::endl;
  }
};

struct LASPP_PACKED ColorData {
  uint16_t red;
  uint16_t green;
  uint16_t blue;

  bool operator==(const ColorData& other) const = default;

  static ColorData RandomData(std::mt19937_64& gen) {
    std::uniform_int_distribution<uint16_t> color_dist(0, std::numeric_limits<uint16_t>::max());
    ColorData color;
    color.red = color_dist(gen);
    color.green = color_dist(gen);
    color.blue = color_dist(gen);
    return color;
  }

  friend std::ostream& operator<<(std::ostream& os, const ColorData& color) {
    os << "RGB: (" << color.red << ", " << color.green << ", " << color.blue << ")" << std::endl;
    return os;
  }
};

struct LASPP_PACKED LASPointFormat2 : LASPointFormat0, ColorData {
  bool operator==(const LASPointFormat2& other) const = default;

  static LASPointFormat2 RandomData(std::mt19937_64& gen) {
    LASPointFormat2 point;
    static_cast<LASPointFormat0&>(point) = LASPointFormat0::RandomData(gen);
    static_cast<ColorData&>(point) = ColorData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat2& lpf2) {
    return os << static_cast<const LASPointFormat0&>(lpf2) << static_cast<const ColorData&>(lpf2)
              << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat3 : LASPointFormat1, ColorData {
  bool operator==(const LASPointFormat3& other) const = default;

  static LASPointFormat3 RandomData(std::mt19937_64& gen) {
    LASPointFormat3 point;
    static_cast<LASPointFormat1&>(point) = LASPointFormat1::RandomData(gen);
    static_cast<ColorData&>(point) = ColorData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat3& lpf3) {
    return os << static_cast<const LASPointFormat1&>(lpf3) << static_cast<const ColorData&>(lpf3)
              << std::endl;
  }
};

struct LASPP_PACKED WavePacketData {
  uint8_t wave_packet_descriptor_index;
  uint64_t byte_offset_to_waveform_data;
  uint32_t wave_packet_size;
  float return_point_waveform_location;
  float x_t;
  float y_t;
  float z_t;

  bool operator==(const WavePacketData& other) const = default;

  static WavePacketData RandomData(std::mt19937_64& gen) {
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    std::uniform_int_distribution<uint64_t> uint64_dist(0, std::numeric_limits<uint64_t>::max());
    std::uniform_int_distribution<uint32_t> uint32_dist(0, std::numeric_limits<uint32_t>::max());
    std::uniform_real_distribution<float> float_dist(-1e6f, 1e6f);

    WavePacketData wave_packet;
    wave_packet.wave_packet_descriptor_index = byte_dist(gen);
    wave_packet.byte_offset_to_waveform_data = uint64_dist(gen);
    wave_packet.wave_packet_size = uint32_dist(gen);
    wave_packet.return_point_waveform_location = float_dist(gen);
    wave_packet.x_t = float_dist(gen);
    wave_packet.y_t = float_dist(gen);
    wave_packet.z_t = float_dist(gen);
    return wave_packet;
  }

  friend std::ostream& operator<<(std::ostream& os, const WavePacketData& wave_packet) {
    os << "Wave packet descriptor index: "
       << static_cast<int>(wave_packet.wave_packet_descriptor_index) << std::endl;
    os << "Byte offset to waveform data: " << wave_packet.byte_offset_to_waveform_data << std::endl;
    os << "Wave packet size: " << wave_packet.wave_packet_size << std::endl;
    os << "Return point waveform location: " << wave_packet.return_point_waveform_location
       << std::endl;
    os << "X_t: " << wave_packet.x_t << std::endl;
    os << "Y_t: " << wave_packet.y_t << std::endl;
    os << "Z_t: " << wave_packet.z_t << std::endl;
    return os;
  }
};

struct LASPP_PACKED LASPointFormat4 : LASPointFormat1, WavePacketData {
  bool operator==(const LASPointFormat4& other) const = default;

  static LASPointFormat4 RandomData(std::mt19937_64& gen) {
    LASPointFormat4 point;
    static_cast<LASPointFormat1&>(point) = LASPointFormat1::RandomData(gen);
    static_cast<WavePacketData&>(point) = WavePacketData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat4& lpf4) {
    return os << static_cast<const LASPointFormat1&>(lpf4)
              << static_cast<const WavePacketData&>(lpf4) << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat5 : LASPointFormat3, WavePacketData {
  bool operator==(const LASPointFormat5& other) const = default;

  static LASPointFormat5 RandomData(std::mt19937_64& gen) {
    LASPointFormat5 point;
    static_cast<LASPointFormat3&>(point) = LASPointFormat3::RandomData(gen);
    static_cast<WavePacketData&>(point) = WavePacketData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat5& lpf5) {
    return os << static_cast<const LASPointFormat3&>(lpf5)
              << static_cast<const WavePacketData&>(lpf5) << std::endl;
  }
};

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
  int16_t scan_angle;
  uint16_t point_source_id;
  double gps_time;

  static LASPointFormat6 RandomData(std::mt19937_64& gen) {
    std::uniform_int_distribution<int32_t> coord_dist(std::numeric_limits<int32_t>::min(),
                                                      std::numeric_limits<int32_t>::max());
    std::uniform_int_distribution<uint16_t> intensity_dist(0, std::numeric_limits<uint16_t>::max());
    std::uniform_int_distribution<uint8_t> return_dist(0, 15);
    std::uniform_int_distribution<uint8_t> class_flags_dist(0, 15);
    std::uniform_int_distribution<uint8_t> scanner_channel_dist(0, 3);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    std::uniform_int_distribution<int16_t> angle_dist(std::numeric_limits<int16_t>::min(),
                                                      std::numeric_limits<int16_t>::max());
    std::uniform_int_distribution<uint16_t> point_source_dist(0,
                                                              std::numeric_limits<uint16_t>::max());
    std::uniform_real_distribution<double> gps_dist(-1e6, 1e6);

    LASPointFormat6 point;
    point.x = coord_dist(gen);
    point.y = coord_dist(gen);
    point.z = coord_dist(gen);
    point.intensity = intensity_dist(gen);
    point.return_number = static_cast<uint8_t>(return_dist(gen) & 0x0F);
    point.number_of_returns = static_cast<uint8_t>(return_dist(gen) & 0x0F);
    point.classification_flags = static_cast<uint8_t>(class_flags_dist(gen) & 0x0F);
    point.scanner_channel = static_cast<uint8_t>(scanner_channel_dist(gen) & 0x03);
    point.scan_direction_flag = static_cast<uint8_t>(byte_dist(gen) & 0x1);
    point.edge_of_flight_line = static_cast<uint8_t>(byte_dist(gen) & 0x1);
    point.classification = static_cast<LASClassification>(
        byte_dist(gen) % (static_cast<uint8_t>(LASClassification::TemporalExclusion) + 1));
    point.user_data = byte_dist(gen);
    point.scan_angle = angle_dist(gen);
    point.point_source_id = point_source_dist(gen);
    point.gps_time = gps_dist(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat6& point) {
    os << "X: " << point.x << std::endl;
    os << "Y: " << point.y << std::endl;
    os << "Z: " << point.z << std::endl;
    os << "Intensity: " << point.intensity << std::endl;
    os << "Return number: " << static_cast<int>(point.return_number) << std::endl;
    os << "Number of returns: " << static_cast<int>(point.number_of_returns) << std::endl;
    os << "Classification flags: " << static_cast<int>(point.classification_flags) << std::endl;
    os << "Scanner channel: " << static_cast<int>(point.scanner_channel) << std::endl;
    os << "Scan direction flag: " << static_cast<int>(point.scan_direction_flag) << std::endl;
    os << "Edge of flight line: " << static_cast<int>(point.edge_of_flight_line) << std::endl;
    os << "Classification: " << point.classification << std::endl;
    os << "User data: " << static_cast<int>(point.user_data) << std::endl;
    os << "Scan angle: " << point.scan_angle << std::endl;
    os << "Point source ID: " << point.point_source_id << std::endl;
    os << "GPS time: " << point.gps_time << std::endl;
    return os;
  }

  bool operator==(const LASPointFormat6& other) const = default;
};

struct LASPP_PACKED LASPointFormat7 : LASPointFormat6, ColorData {
  bool operator==(const LASPointFormat7& other) const = default;

  static LASPointFormat7 RandomData(std::mt19937_64& gen) {
    LASPointFormat7 point;
    static_cast<LASPointFormat6&>(point) = LASPointFormat6::RandomData(gen);
    static_cast<ColorData&>(point) = ColorData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat7& lpf7) {
    return os << static_cast<const LASPointFormat6&>(lpf7) << static_cast<const ColorData&>(lpf7)
              << std::endl;
  }
};

struct LASPP_PACKED NIRData {
  bool operator==(const NIRData& other) const = default;
  uint16_t NIR;

  static NIRData RandomData(std::mt19937_64& gen) {
    std::uniform_int_distribution<uint16_t> nir_dist(0, std::numeric_limits<uint16_t>::max());
    NIRData nir;
    nir.NIR = nir_dist(gen);
    return nir;
  }

  friend std::ostream& operator<<(std::ostream& os, const NIRData& nir) {
    os << "NIR: " << nir.NIR << std::endl;
    return os;
  }
};

struct LASPP_PACKED LASPointFormat8 : LASPointFormat7, NIRData {
  bool operator==(const LASPointFormat8& other) const = default;

  static LASPointFormat8 RandomData(std::mt19937_64& gen) {
    LASPointFormat8 point;
    static_cast<LASPointFormat7&>(point) = LASPointFormat7::RandomData(gen);
    static_cast<NIRData&>(point) = NIRData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat8& lpf8) {
    return os << static_cast<const LASPointFormat7&>(lpf8) << static_cast<const NIRData&>(lpf8)
              << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat9 : LASPointFormat6, WavePacketData {
  bool operator==(const LASPointFormat9& other) const = default;

  static LASPointFormat9 RandomData(std::mt19937_64& gen) {
    LASPointFormat9 point;
    static_cast<LASPointFormat6&>(point) = LASPointFormat6::RandomData(gen);
    static_cast<WavePacketData&>(point) = WavePacketData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat9& lpf9) {
    return os << static_cast<const LASPointFormat6&>(lpf9)
              << static_cast<const WavePacketData&>(lpf9) << std::endl;
  }
};

struct LASPP_PACKED LASPointFormat10 : LASPointFormat9, ColorData, NIRData {
  bool operator==(const LASPointFormat10& other) const = default;

  static LASPointFormat10 RandomData(std::mt19937_64& gen) {
    LASPointFormat10 point;
    static_cast<LASPointFormat9&>(point) = LASPointFormat9::RandomData(gen);
    static_cast<ColorData&>(point) = ColorData::RandomData(gen);
    static_cast<NIRData&>(point) = NIRData::RandomData(gen);
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat10& lpf10) {
    return os << static_cast<const LASPointFormat9&>(lpf10) << static_cast<const ColorData&>(lpf10)
              << static_cast<const NIRData&>(lpf10) << std::endl;
  }
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
  LASPP_FAIL("Invalid point format", format);

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

static_assert(std::is_trivial_v<LASPointFormat0>);
static_assert(std::is_trivial_v<LASPointFormat1>);
static_assert(std::is_trivial_v<LASPointFormat2>);
static_assert(std::is_trivial_v<LASPointFormat3>);
static_assert(std::is_trivial_v<LASPointFormat4>);
static_assert(std::is_trivial_v<LASPointFormat5>);
static_assert(std::is_trivial_v<LASPointFormat6>);
static_assert(std::is_trivial_v<LASPointFormat7>);
static_assert(std::is_trivial_v<LASPointFormat8>);
static_assert(std::is_trivial_v<LASPointFormat9>);
static_assert(std::is_trivial_v<LASPointFormat10>);

constexpr std::array<uint16_t, 11> LASPointFormatSize = {
    {sizeof(LASPointFormat0), sizeof(LASPointFormat1), sizeof(LASPointFormat2),
     sizeof(LASPointFormat3), sizeof(LASPointFormat4), sizeof(LASPointFormat5),
     sizeof(LASPointFormat6), sizeof(LASPointFormat7), sizeof(LASPointFormat8),
     sizeof(LASPointFormat9), sizeof(LASPointFormat10)}};

inline uint16_t size_of_point_format(uint8_t format) {
  return LASPointFormatSize[format & (~(1u << 7))];
}

template <typename T1, typename T2, typename = void>
struct CopyAssignable : std::false_type {};

template <typename T1, typename T2>
struct CopyAssignable<T1, T2, decltype(void(std::declval<T1>() = std::declval<T2>()))>
    : std::true_type {};

template <typename T1, typename T2>
using is_copy_assignable = CopyAssignable<T1, T2>;

template <typename T1, typename T2, typename = void>
struct CopyFromable : std::false_type {};

template <typename T1, typename T2>
struct CopyFromable<T1, T2, decltype(copy_from(std::declval<T1&>(), std::declval<const T2&>()))>
    : std::true_type {};

template <typename T1, typename T2>
using is_copy_fromable = CopyFromable<T1, T2>;

template <typename T1, typename T2>
constexpr bool is_convertable() {
  return is_copy_assignable<T2, T1>::value || std::is_base_of_v<T2, T1> ||
         is_copy_fromable<T1, T2>::value;
}

#pragma pack(pop)

}  // namespace laspp
