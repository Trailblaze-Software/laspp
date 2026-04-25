/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <istream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#include "utilities/assert.hpp"

namespace laspp {

// Kept for backward-compatibility; no longer used in the hot decompression path.
// Read-only stream buffer for const data. We ensure safety by never calling setp() or any write
// operations.
class PointerStreamBuffer : public std::streambuf {
 public:
  explicit PointerStreamBuffer(std::span<const std::byte> data) {
    // setg() requires non-const pointers, but only uses them for read positioning
    setg(const_cast<char*>(reinterpret_cast<const char*>(data.data())),
         const_cast<char*>(reinterpret_cast<const char*>(data.data())),
         const_cast<char*>(reinterpret_cast<const char*>(data.data() + data.size())));
  }

  // Explicitly disable write operations to ensure const-correctness
  std::streambuf::int_type overflow(std::streambuf::int_type) override {
#ifdef LASPP_DEBUG_ASSERTS
    LASPP_FAIL("PointerStreamBuffer: write operation attempted on read-only buffer");
#else
    // Return EOF to indicate failure (standard library convention)
    return traits_type::eof();
#endif
  }

  // Override xsgetn to ensure proper bounds checking when reading
  std::streamsize xsgetn(char* s, std::streamsize n) override {
    char* base = eback();
    char* current = gptr();
    char* end = egptr();
    if (current == nullptr || end == nullptr || base == nullptr) {
      return 0;  // No data available
    }

    std::streamsize available = end - current;
    std::streamsize to_read = (n < available) ? n : available;

    if (to_read > 0) {
      std::memcpy(s, current, static_cast<size_t>(to_read));
      // Update get pointer
      setg(base, current + to_read, end);
    }

    // Return the number of bytes actually read
    // If to_read < n, LASPP_CHECK_READ will detect this via gcount()
    return to_read;
  }

 protected:
  // Support seekg() / tellg() so that LASReader can seek within mapped memory.
  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which = std::ios_base::in) override {
    if (which & std::ios_base::out) return pos_type(off_type(-1));  // read-only buffer

    // Compute offsets as integers to avoid undefined behavior from pointer arithmetic
    const char* base = eback();
    const char* current = gptr();
    const char* end = egptr();
    ptrdiff_t buffer_size = end - base;

    ptrdiff_t new_index;
    if (dir == std::ios_base::beg) {
      new_index = off;
    } else if (dir == std::ios_base::cur) {
      new_index = (current - base) + off;
    } else {  // std::ios_base::end
      new_index = buffer_size + off;
    }

    // Validate bounds before converting to pointer
    if (new_index < 0 || new_index > buffer_size) {
      return pos_type(off_type(-1));
    }

    // Safe to convert to pointer now
    char* new_gptr = const_cast<char*>(base) + new_index;
    setg(const_cast<char*>(base), new_gptr, const_cast<char*>(end));
    return pos_type(new_index);
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override {
    return seekoff(off_type(pos), std::ios_base::beg, which);
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

  [[nodiscard]] std::byte read_byte() {
    if (m_ptr >= m_end) [[unlikely]]
      return std::byte{0};
    return *m_ptr++;
  }

 public:
  InStream(const InStream&) = delete;
  InStream& operator=(const InStream&) = delete;
  InStream(InStream&&) = delete;
  InStream& operator=(InStream&&) = delete;

  // Fast path: construct directly from in-memory data.  No copies, no virtual dispatch.
  // Requires at least 4 bytes to initialize the range coder state.
  InStream(const std::byte* data, size_t size) : m_ptr(data), m_end(data + size) {
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
    LASPP_CHECK_SEEK(stream, 0, std::ios::end);
    auto end_pos = stream.tellg();
    LASPP_CHECK_SEEK(stream, cur, std::ios::beg);
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
  //
  // The bounds checks use [[unlikely]] so the compiler keeps them out of the hot path and
  // the branch predictor treats them as never-taken.
  uint32_t get_value() {
    if (m_length < (1u << 8)) {
      m_value <<= 24;
      m_length <<= 24;
      // Read 3 bytes big-endian; treat out-of-bounds bytes as zero.
      auto b0 = (m_ptr + 0 < m_end) ? static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[0])) : 0u;
      auto b1 = (m_ptr + 1 < m_end) ? static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[1])) : 0u;
      auto b2 = (m_ptr + 2 < m_end) ? static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[2])) : 0u;
      m_value |= (b0 << 16) | (b1 << 8) | b2;
      m_ptr += 3;
    } else if (m_length < (1u << 16)) {
      m_value <<= 16;
      m_length <<= 16;
      // Read 2 bytes big-endian; treat out-of-bounds bytes as zero.
      auto b0 = (m_ptr + 0 < m_end) ? static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[0])) : 0u;
      auto b1 = (m_ptr + 1 < m_end) ? static_cast<uint32_t>(static_cast<uint8_t>(m_ptr[1])) : 0u;
      m_value |= (b0 << 8) | b1;
      m_ptr += 2;
    } else if (m_length < (1u << 24)) {
      m_value <<= 8;
      m_length <<= 8;
      // Read 1 byte; treat out-of-bounds as zero.
      if (m_ptr < m_end) [[likely]]
        m_value |= static_cast<uint32_t>(static_cast<uint8_t>(*m_ptr++));
      else
        m_ptr++;
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
    LASPP_CHECK_SEEK(m_stream, updated_p, std::ios::beg);
    uint8_t carry = static_cast<uint8_t>(m_stream.get());
    m_stream.seekp(updated_p);
    while (carry == 0xff) {
      m_stream.put(0);
      LASPP_ASSERT_GT(updated_p, 0);
      updated_p--;
      LASPP_CHECK_SEEK(m_stream, updated_p, std::ios::beg);
      carry = static_cast<uint8_t>(m_stream.get());
      m_stream.seekp(updated_p);
    }
    m_stream.put(static_cast<char>(carry + 1));
    m_stream.seekp(current_p);
    LASPP_CHECK_SEEK(m_stream, current_g, std::ios::beg);
  }

  void update_range(uint32_t lower, uint32_t upper) {
    LASPP_DEBUG_ASSERT(!m_finalized);
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
