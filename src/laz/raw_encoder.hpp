#pragma once

#include "laz/stream.hpp"

namespace laspp {

inline uint64_t raw_decode(InStream& in_stream, uint8_t n_bits) {
  if (n_bits > 19) {
    return raw_decode(in_stream, 16) + (raw_decode(in_stream, n_bits - 16) * 1ul << 16);
  }

  uint32_t resolution = in_stream.length() / (1u << n_bits);
  uint32_t raw = in_stream.get_value() / resolution;

  in_stream.update_range(resolution * raw, resolution * raw + resolution);

  in_stream.get_value();
  return raw;
}

inline void raw_encode(OutStream& out_stream, uint64_t bits, uint8_t n_bits) {
  if (n_bits > 19) {
    raw_encode(out_stream, bits % (1u << 16), 16);
    raw_encode(out_stream, bits >> 16, n_bits - 16);
    return;
  }

  uint32_t resolution = out_stream.length() / (1u << n_bits);

  out_stream.update_range(bits * resolution, bits * resolution + resolution);
  out_stream.get_base();
}
}  // namespace laspp
