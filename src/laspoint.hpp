
#pragma once

#include <cstdint>
#include <iostream>

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

struct __attribute__((packed)) LASPointFormat0 {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t intensity;
  uint8_t return_number : 3;
  uint8_t number_of_returns : 3;
  uint8_t scan_direction_flag : 1;
  uint8_t edge_of_flight_line : 1;
  LASClassification classification : 5;
  uint8_t synthetic : 1;
  uint8_t key_point : 1;
  uint8_t withheld : 1;
  uint8_t scan_angle_rank;
  uint8_t user_data;
  uint16_t point_source_id;

  friend std::ostream& operator<<(std::ostream& os, const LASPointFormat0& point) {
    os << "X: " << point.x << std::endl;
    os << "Y: " << point.y << std::endl;
    os << "Z: " << point.z << std::endl;
    os << "Intensity: " << point.intensity << std::endl;
    os << "Return number: " << (uint)point.return_number << std::endl;
    os << "Number of returns: " << (uint)point.number_of_returns << std::endl;
    os << "Scan direction flag: " << (uint)point.scan_direction_flag << std::endl;
    os << "Edge of flight line: " << (uint)point.edge_of_flight_line << std::endl;
    os << "Classification: " << point.classification << std::endl;
    os << "Synthetic: " << (uint)point.synthetic << std::endl;
    os << "Key point: " << (uint)point.key_point << std::endl;
    os << "Withheld: " << (uint)point.withheld << std::endl;
    os << "Scan angle rank: " << (uint)point.scan_angle_rank << std::endl;
    os << "User data: " << (uint)point.user_data << std::endl;
    os << "Point source ID: " << point.point_source_id << std::endl;
    return os;
  }
};

struct __attribute__((packed)) GPSTime {
  double gps_time;
};

struct __attribute__((packed)) LASPointFormat1 : LASPointFormat0, GPSTime {};

struct __attribute__((packed)) ColorData {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
};

struct __attribute__((packed)) LASPointFormat2 : LASPointFormat0, ColorData {};

struct __attribute__((packed)) LASPointFormat3 : LASPointFormat1, ColorData {};

struct __attribute__((packed)) WavePacketData {
  uint8_t wave_packet_descriptor_index;
  uint64_t byte_offset_to_waveform_data;
  uint32_t wave_packet_size;
  float return_point_waveform_location;
  float x_t;
  float y_t;
  float z_t;
};

struct __attribute__((packed)) LASPointFormat4 : LASPointFormat1, WavePacketData {};

struct __attribute__((packed)) LASPointFormat5 : LASPointFormat3, WavePacketData {};

struct __attribute__((packed)) LASPointFormat6 {
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
};

struct __attribute__((packed)) LASPointFormat7 : LASPointFormat6, ColorData {};

struct __attribute__((packed)) LASPointFormat8 : LASPointFormat7 {
  uint16_t NIR;
};

struct __attribute__((packed)) LASPointFormat9 : LASPointFormat6, WavePacketData {};

struct __attribute__((packed)) LASPointFormat10 : LASPointFormat9, ColorData {
  uint16_t NIR;
};

constexpr size_t LASPointFormatSize[] = {
    sizeof(LASPointFormat0), sizeof(LASPointFormat1), sizeof(LASPointFormat2),
    sizeof(LASPointFormat3), sizeof(LASPointFormat4), sizeof(LASPointFormat5),
    sizeof(LASPointFormat6), sizeof(LASPointFormat7), sizeof(LASPointFormat8),
    sizeof(LASPointFormat9), sizeof(LASPointFormat10)};

#pragma pack(pop)

}  // namespace laspp
