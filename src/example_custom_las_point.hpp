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

struct ExampleFullLASPoint {
  std::array<int32_t, 3> position;
  double gps_time;

  void operator=(const laspp::LASPointFormat0& point) {
    position[0] = point.x;
    position[1] = point.y;
    position[2] = point.z;
  }

  void operator=(const laspp::GPSTime& point) { gps_time = point; }

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

  operator laspp::GPSTime() const { return laspp::GPSTime(gps_time); }

  friend std::ostream& operator<<(std::ostream& os, const ExampleFullLASPoint& point) {
    os << "Position: (" << point.position[0] << ", " << point.position[1] << ", "
       << point.position[2] << ")";
    os << " GPS Time: " << point.gps_time;
    return os;
  }
};
