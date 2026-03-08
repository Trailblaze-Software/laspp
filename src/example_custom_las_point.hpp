/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>
#include <ostream>

#include "las_point.hpp"

struct ExampleMinimalLASPoint {
  std::array<int32_t, 3> position;

  void operator=(const laspp::LASPointFormat0& point) {
    position[0] = point.x;
    position[1] = point.y;
    position[2] = point.z;
  }

  operator laspp::LASPointFormat0() const {
    laspp::LASPointFormat0 point;
    point.x = position[0];
    point.y = position[1];
    point.z = position[2];
    point.intensity = 0;
    point.bit_byte = 0;
    point.classification_byte = 0;
    point.scan_angle_rank = 0;
    point.user_data = 0;
    point.point_source_id = 0;
    return point;
  }

  friend std::ostream& operator<<(std::ostream& os, const ExampleMinimalLASPoint& point) {
    os << "Position: (" << point.position[0] << ", " << point.position[1] << ", "
       << point.position[2] << ")";
    return os;
  }
};

struct ExampleFullLASPoint : ExampleMinimalLASPoint {
  double gps_time;

  void operator=(const laspp::LASPointFormat0& point) { ExampleMinimalLASPoint::operator=(point); }

  friend std::ostream& operator<<(std::ostream& os, const ExampleFullLASPoint& point) {
    os << "Position: (" << point.position[0] << ", " << point.position[1] << ", "
       << point.position[2] << ")";
    os << " GPS Time: " << point.gps_time;
    return os;
  }
};

inline void copy_from(ExampleFullLASPoint& dest, const laspp::GPSTime& src) {
  dest.gps_time = src.gps_time.f64;
}

inline void copy_from(laspp::GPSTime& dest, const ExampleFullLASPoint& src) {
  dest.gps_time.f64 = src.gps_time;
}
