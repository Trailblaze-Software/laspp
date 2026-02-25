/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
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

#include <cstdint>

namespace laspp {

inline int32_t wrapping_int32_add(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

inline int32_t wrapping_int32_sub(int32_t a, int32_t b) { return wrapping_int32_add(a, -b); }

inline uint8_t clamp(uint8_t value, int delta) {
  if (delta > 255 - value) {
    return 255;
  } else if (value < -delta) {
    return 0;
  }
  return static_cast<uint8_t>(value + static_cast<uint32_t>(delta));
}

}  // namespace laspp
