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

#include "laz/stream.hpp"

namespace laspp {

inline uint64_t raw_decode(InStream& in_stream, uint8_t n_bits) {
  if (n_bits > 19) {
    uint64_t raw = raw_decode(in_stream, uint8_t{16});
    uint64_t raw2 = raw_decode(in_stream, static_cast<uint8_t>(n_bits - 16));
    return raw + (raw2 << 16);
  }

  uint32_t value = in_stream.get_value();
  uint32_t resolution = in_stream.length() / (1u << n_bits);
  uint32_t raw = value / resolution;

  in_stream.update_range(resolution * raw, resolution * raw + resolution);

  return raw;
}

inline void raw_encode(OutStream& out_stream, uint64_t bits, uint8_t n_bits) {
  if (n_bits > 19) {
    raw_encode(out_stream, bits % (1u << 16), uint8_t{16});
    raw_encode(out_stream, bits >> 16, static_cast<uint8_t>(n_bits - 16));
    return;
  }

  uint32_t resolution = out_stream.length() / (1u << n_bits);

  uint32_t bits_32 = static_cast<uint32_t>(bits);
  out_stream.update_range(bits_32 * resolution, bits_32 * resolution + resolution);
}
}  // namespace laspp
