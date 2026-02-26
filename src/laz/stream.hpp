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

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <istream>
#include <limits>
#include <vector>

#include "utilities/assert.hpp"

namespace laspp {

// Kept for backward-compatibility; no longer used in the hot decompression path.
class PointerStreamBuffer : public std::streambuf {
 public:
  PointerStreamBuffer(std::byte* data, size_t size) {
    setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data),
         reinterpret_cast<char*>(data + size));
  }
};

struct StreamVariables {
  uint32_t m_base = 0;
  uint32_t m_value = 0;
  uint32_t m_length = std::numeric_limits<uint32_t>::max();
};

// Arithmetic-coding input stream.
//
// The fast constructor takes a raw pointer to already-in-memory compressed data and reads
// directly from it — no virtual dispatch, no intermediate buffer.  This is the path used
// by all LAZ chunk decompression.
//
// The legacy std::istream& constructor slurps all remaining stream data into an owned
// buffer and then uses the same pointer-based read path; it exists for tests and any
// callers that work with file streams.
class InStream : StreamVariables {
  const std::byte* m_ptr = nullptr;
  const std::byte* m_end = nullptr;  // End of buffer for bounds checking

  // Owned buffer used only by the std::istream& constructor.
  std::vector<std::byte> m_owned_data;

  [[nodiscard]] std::byte read_byte() noexcept {
    LASPP_ASSERT(m_ptr < m_end, "InStream: read past end of buffer");
    return *m_ptr++;
  }

 public:
  InStream(const InStream&) = delete;
  InStream& operator=(const InStream&) = delete;
  InStream(InStream&&) = delete;
  InStream& operator=(InStream&&) = delete;

  // Fast path: construct directly from in-memory data.  No copies, no virtual dispatch.
  // Requires at least 4 bytes to initialize the range coder state.
  InStream(const std::byte* data, size_t size) noexcept : m_ptr(data), m_end(data + size) {
    LASPP_ASSERT_GE(size, 4u, "InStream: buffer too small (need at least 4 bytes)");
    for (int i = 0; i < 4; i++) {
      m_value <<= 8;
      m_value |= static_cast<uint32_t>(static_cast<uint8_t>(read_byte()));
    }
    m_length = std::numeric_limits<uint32_t>::max();
  }

  // Legacy path for tests and streaming callers: reads all remaining data from `stream`
  // into an owned buffer, then proceeds as the pointer constructor.
  explicit InStream(std::istream& stream) {
    auto cur = stream.tellg();
    stream.seekg(0, std::ios::end);
    auto end_pos = stream.tellg();
    stream.seekg(cur);
    size_t sz = static_cast<size_t>(end_pos - cur);
    LASPP_ASSERT_GE(sz, 4u, "InStream: stream too small (need at least 4 bytes)");
    m_owned_data.resize(sz);
    stream.read(reinterpret_cast<char*>(m_owned_data.data()), static_cast<std::streamsize>(sz));
    m_ptr = m_owned_data.data();
    m_end = m_owned_data.data() + sz;
    for (int i = 0; i < 4; i++) {
      m_value <<= 8;
      m_value |= static_cast<uint32_t>(static_cast<uint8_t>(read_byte()));
    }
    m_length = std::numeric_limits<uint32_t>::max();
  }

  uint32_t length() const noexcept { return m_length; }

  void update_range(uint32_t lower, uint32_t upper) noexcept {
    m_value -= lower;
    m_length = upper - lower;
  }

  // Renormalise the range coder and return the current value.  Reads 0, 1, 2 or 3 bytes
  // from the in-memory pointer — the common case (length >= 2^24) reads nothing, so this
  // is very cheap on average.
  uint32_t get_value() noexcept {
    if (m_length < (1u << 8)) {
      LASPP_ASSERT(m_ptr + 3 <= m_end, "InStream: read past end of buffer (need 3 bytes)");
      m_value <<= 24;
      m_length <<= 24;
      // Read 3 bytes big-endian.
      m_value |= (static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[0])) << 16) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[1])) << 8) |
                 static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[2]));
      m_ptr += 3;
    } else if (m_length < (1u << 16)) {
      LASPP_ASSERT(m_ptr + 2 <= m_end, "InStream: read past end of buffer (need 2 bytes)");
      m_value <<= 16;
      m_length <<= 16;
      // Read 2 bytes big-endian.
      m_value |= (static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[0])) << 8) |
                 static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[1]));
      m_ptr += 2;
    } else if (m_length < (1u << 24)) {
      LASPP_ASSERT(m_ptr < m_end, "InStream: read past end of buffer (need 1 byte)");
      m_value <<= 8;
      m_length <<= 8;
      m_value |= static_cast<uint32_t>(static_cast<uint8_t>(*m_ptr++));
    }
    return m_value;
  }
};

class OutStream : StreamVariables {
  std::iostream& m_stream;
  bool m_finalized;

 public:
  OutStream(const OutStream&) = delete;
  OutStream& operator=(const OutStream&) = delete;
  OutStream(OutStream&&) = delete;
  OutStream& operator=(OutStream&&) = delete;

 private:
  void finalize_stream() {
    bool write_two_bytes;
    if (length() > (1u << 25)) {
      update_range(1u << 24, 0b11u << 23);
      write_two_bytes = false;
    } else {
      update_range(1u << 23, (1u << 23) + (1u << 15));
      write_two_bytes = true;
    }
    m_stream.put(0);
    m_stream.put(0);
    if (!write_two_bytes) {
      m_stream.put(0);
    }
  }

  void update_base() {
    while (length() < (1 << 24)) {
      m_stream.put(static_cast<char>(m_base >> 24));
      m_base <<= 8;
      m_length <<= 8;
    }
  }

 public:
  explicit OutStream(std::iostream& stream) : m_stream(stream), m_finalized(false) {
    m_base = 0;
    m_length = std::numeric_limits<uint32_t>::max();
  }

  ~OutStream() { finalize(); }

  void finalize() {
    if (!m_finalized) {
      finalize_stream();
      m_finalized = true;
    }
  }

  uint32_t length() const { return m_length; }

  void propogate_carry() {
    const int64_t current_g = m_stream.tellg();
    const int64_t current_p = m_stream.tellp();
    int64_t updated_p = current_p - 1;
    m_stream.seekg(updated_p);
    uint8_t carry = static_cast<uint8_t>(m_stream.get());
    m_stream.seekp(updated_p);
    while (carry == 0xff) {
      m_stream.put(0);
      LASPP_ASSERT_GT(updated_p, 0);
      updated_p--;
      m_stream.seekg(updated_p);
      carry = static_cast<uint8_t>(m_stream.get());
      m_stream.seekp(updated_p);
    }
    m_stream.put(static_cast<char>(carry + 1));
    m_stream.seekp(current_p);
    m_stream.seekg(current_g);
  }

  void update_range(uint32_t lower, uint32_t upper) {
    LASPP_ASSERT(!m_finalized);
    if ((static_cast<uint64_t>(m_base) + static_cast<uint64_t>(lower)) >=
        (static_cast<uint64_t>(1) << 32)) {
      propogate_carry();
    }
    m_base += lower;
    m_length = upper - lower;
    update_base();
  }

  uint32_t get_base() const { return m_base; }
};
}  // namespace laspp
