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

#include "laz/stream.hpp"

namespace laspp {

inline uint64_t raw_decode(InStream& in_stream, uint8_t n_bits) {
  if (n_bits > 19) {
    return raw_decode(in_stream, 16) + (raw_decode(in_stream, n_bits - 16) * 1ul << 16);
  }

  uint32_t value = in_stream.get_value();
  uint32_t resolution = in_stream.length() / (1u << n_bits);
  uint32_t raw = value / resolution;

  in_stream.update_range(resolution * raw, resolution * raw + resolution);

  return raw;
}

inline void raw_encode(OutStream& out_stream, uint64_t bits, uint8_t n_bits) {
  if (n_bits > 19) {
    raw_encode(out_stream, bits % (1u << 16), 16);
    raw_encode(out_stream, bits >> 16, n_bits - 16);
    return;
  }

  out_stream.get_base();
  uint32_t resolution = out_stream.length() / (1u << n_bits);

  out_stream.update_range(bits * resolution, bits * resolution + resolution);
}
}  // namespace laspp
