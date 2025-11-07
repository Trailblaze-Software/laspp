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

#include <limits>
#include <sstream>

#include "las_header.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test default constructor
  {
    Bound2D bound;
    LASPP_ASSERT_EQ(bound.min_x(), std::numeric_limits<double>::max());
    LASPP_ASSERT_EQ(bound.min_y(), std::numeric_limits<double>::max());
    LASPP_ASSERT_EQ(bound.max_x(), std::numeric_limits<double>::lowest());
    LASPP_ASSERT_EQ(bound.max_y(), std::numeric_limits<double>::lowest());
  }

  // Test parameterized constructor
  {
    Bound2D bound(10.0, 20.0, 30.0, 40.0);
    LASPP_ASSERT_EQ(bound.min_x(), 10.0);
    LASPP_ASSERT_EQ(bound.min_y(), 20.0);
    LASPP_ASSERT_EQ(bound.max_x(), 30.0);
    LASPP_ASSERT_EQ(bound.max_y(), 40.0);
  }

  // Test update method
  {
    Bound2D bound;
    bound.update(5.0, 10.0);
    LASPP_ASSERT_EQ(bound.min_x(), 5.0);
    LASPP_ASSERT_EQ(bound.min_y(), 10.0);
    LASPP_ASSERT_EQ(bound.max_x(), 5.0);
    LASPP_ASSERT_EQ(bound.max_y(), 10.0);

    bound.update(15.0, 20.0);
    LASPP_ASSERT_EQ(bound.min_x(), 5.0);
    LASPP_ASSERT_EQ(bound.min_y(), 10.0);
    LASPP_ASSERT_EQ(bound.max_x(), 15.0);
    LASPP_ASSERT_EQ(bound.max_y(), 20.0);

    bound.update(3.0, 25.0);
    LASPP_ASSERT_EQ(bound.min_x(), 3.0);
    LASPP_ASSERT_EQ(bound.min_y(), 10.0);
    LASPP_ASSERT_EQ(bound.max_x(), 15.0);
    LASPP_ASSERT_EQ(bound.max_y(), 25.0);
  }

  // Test update with multiple points
  {
    Bound2D bound;
    bound.update(0.0, 0.0);
    bound.update(10.0, 5.0);
    bound.update(5.0, 10.0);
    bound.update(-5.0, -10.0);
    bound.update(15.0, 20.0);

    LASPP_ASSERT_EQ(bound.min_x(), -5.0);
    LASPP_ASSERT_EQ(bound.min_y(), -10.0);
    LASPP_ASSERT_EQ(bound.max_x(), 15.0);
    LASPP_ASSERT_EQ(bound.max_y(), 20.0);
  }

  // Test stream output
  {
    Bound2D bound(1.5, 2.5, 3.5, 4.5);
    std::stringstream ss;
    ss << bound;
    std::string output = ss.str();
    LASPP_ASSERT(output.find("1.5") != std::string::npos);
    LASPP_ASSERT(output.find("2.5") != std::string::npos);
    LASPP_ASSERT(output.find("3.5") != std::string::npos);
    LASPP_ASSERT(output.find("4.5") != std::string::npos);
  }

  // Test with negative coordinates
  {
    Bound2D bound(-10.0, -20.0, -5.0, -15.0);
    LASPP_ASSERT_EQ(bound.min_x(), -10.0);
    LASPP_ASSERT_EQ(bound.min_y(), -20.0);
    LASPP_ASSERT_EQ(bound.max_x(), -5.0);
    LASPP_ASSERT_EQ(bound.max_y(), -15.0);

    bound.update(-15.0, -25.0);
    LASPP_ASSERT_EQ(bound.min_x(), -15.0);
    LASPP_ASSERT_EQ(bound.min_y(), -25.0);
    LASPP_ASSERT_EQ(bound.max_x(), -5.0);
    LASPP_ASSERT_EQ(bound.max_y(), -15.0);
  }

  // Test with very large coordinates
  {
    double large = 1e10;
    Bound2D bound(-large, -large, large, large);
    LASPP_ASSERT_EQ(bound.min_x(), -large);
    LASPP_ASSERT_EQ(bound.min_y(), -large);
    LASPP_ASSERT_EQ(bound.max_x(), large);
    LASPP_ASSERT_EQ(bound.max_y(), large);
  }

  // Test with zero-size bounds (point)
  {
    Bound2D bound;
    bound.update(5.0, 10.0);
    bound.update(5.0, 10.0);  // Same point
    LASPP_ASSERT_EQ(bound.min_x(), 5.0);
    LASPP_ASSERT_EQ(bound.min_y(), 10.0);
    LASPP_ASSERT_EQ(bound.max_x(), 5.0);
    LASPP_ASSERT_EQ(bound.max_y(), 10.0);
  }

  // Test with zero coordinates
  {
    Bound2D bound(0.0, 0.0, 0.0, 0.0);
    LASPP_ASSERT_EQ(bound.min_x(), 0.0);
    LASPP_ASSERT_EQ(bound.min_y(), 0.0);
    LASPP_ASSERT_EQ(bound.max_x(), 0.0);
    LASPP_ASSERT_EQ(bound.max_y(), 0.0);
  }

  return 0;
}
