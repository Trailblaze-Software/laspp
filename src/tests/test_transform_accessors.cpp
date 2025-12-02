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

#include "las_header.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test non-const Transform::offsets() and scale_factors()
  {
    Transform transform;

    // Test non-const offsets()
    transform.offsets().x() = 100.0;
    transform.offsets().y() = 200.0;
    transform.offsets().z() = 300.0;

    LASPP_ASSERT_EQ(transform.offsets().x(), 100.0);
    LASPP_ASSERT_EQ(transform.offsets().y(), 200.0);
    LASPP_ASSERT_EQ(transform.offsets().z(), 300.0);

    // Test non-const scale_factors()
    transform.scale_factors().x() = 0.01;
    transform.scale_factors().y() = 0.02;
    transform.scale_factors().z() = 0.03;

    LASPP_ASSERT_EQ(transform.scale_factors().x(), 0.01);
    LASPP_ASSERT_EQ(transform.scale_factors().y(), 0.02);
    LASPP_ASSERT_EQ(transform.scale_factors().z(), 0.03);
  }

  // Test through LASHeader
  {
    LASHeader header;

    // Test non-const access through header
    header.transform().offsets().x() = 500.0;
    header.transform().offsets().y() = 600.0;
    header.transform().offsets().z() = 700.0;

    LASPP_ASSERT_EQ(header.transform().offsets().x(), 500.0);
    LASPP_ASSERT_EQ(header.transform().offsets().y(), 600.0);
    LASPP_ASSERT_EQ(header.transform().offsets().z(), 700.0);

    header.transform().scale_factors().x() = 0.1;
    header.transform().scale_factors().y() = 0.2;
    header.transform().scale_factors().z() = 0.3;

    LASPP_ASSERT_EQ(header.transform().scale_factors().x(), 0.1);
    LASPP_ASSERT_EQ(header.transform().scale_factors().y(), 0.2);
    LASPP_ASSERT_EQ(header.transform().scale_factors().z(), 0.3);
  }

  // Test const access still works
  {
    Transform transform;
    transform.offsets().x() = 10.0;
    transform.scale_factors().x() = 0.5;

    const Transform& const_transform = transform;
    LASPP_ASSERT_EQ(const_transform.offsets().x(), 10.0);
    LASPP_ASSERT_EQ(const_transform.scale_factors().x(), 0.5);
  }

  return 0;
}
