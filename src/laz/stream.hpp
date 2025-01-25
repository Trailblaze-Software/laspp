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
#include <bit>
#include <cstdint>
#include <iostream>
#include <istream>
#include <limits>

#include "utilities/assert.hpp"

namespace laspp {

class PointerStreamBuffer : public std::streambuf {
 public:
  PointerStreamBuffer(std::byte* data, size_t size) {
    setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data),
         reinterpret_cast<char*>(data + size));
  }
};

#define BORING_VERSION

struct StreamVariables {
  uint32_t m_base = 0;
  uint32_t m_value = 0;
  uint32_t m_length = std::numeric_limits<uint32_t>::max();
};

#ifdef BORING_VERSION
class InStream : StreamVariables {
  constexpr static size_t BUFFER_SIZE = 1024;
  std::istream& m_stream;
  std::array<std::byte, BUFFER_SIZE> m_buffer;
  size_t buffer_idx;

  std::byte read_byte() {
    if (buffer_idx == BUFFER_SIZE) {
      m_stream.read(reinterpret_cast<char*>(m_buffer.data()), BUFFER_SIZE);
      if (!m_stream) {
        m_stream.clear();
      }
      buffer_idx = 0;
    }
    return m_buffer[buffer_idx++];
  }

  template <size_t Extent>
  void read_bytes(std::span<std::byte, Extent> bytes) {
    if constexpr (Extent == 1) {
      bytes[0] = read_byte();
      return;
    } else if constexpr (Extent == 2) {
      if (buffer_idx + 2 < BUFFER_SIZE) {
        bytes[0] = m_buffer[buffer_idx++];
        bytes[1] = m_buffer[buffer_idx++];
      } else {
        bytes[0] = read_byte();
        bytes[1] = read_byte();
      }
      return;
    } else if constexpr (Extent == 3) {
      if (buffer_idx + 3 < BUFFER_SIZE) {
        bytes[0] = m_buffer[buffer_idx++];
        bytes[1] = m_buffer[buffer_idx++];
        bytes[2] = m_buffer[buffer_idx++];
      } else {
        bytes[0] = read_byte();
        bytes[1] = read_byte();
        bytes[2] = read_byte();
      }
      return;
    } else {
      static_assert(Extent == 1);
    }
  }

 public:
  explicit InStream(std::istream& stream) : m_stream(stream), m_buffer(), buffer_idx(BUFFER_SIZE) {
    for (int i = 0; i < 4; i++) {
      m_value <<= 8;
      m_value |= static_cast<uint8_t>(read_byte());
    }
    m_length = std::numeric_limits<uint32_t>::max();
  }

  uint32_t length() const { return m_length; }

  void update_range(uint32_t lower, uint32_t upper) {
    m_value -= lower;
    m_length = upper - lower;
  }

  uint32_t get_value() {
    if (length() < (1 << 8)) {
      m_value <<= 24;
      m_length <<= 24;
      uint32_t new_3_bytes = 0;
      std::array<std::byte, 3>& bla = *reinterpret_cast<std::array<std::byte, 3>*>(&new_3_bytes);
      read_bytes(std::span<std::byte, 3>(bla));
      std::swap(bla[0], bla[2]);
      m_value |= new_3_bytes;
    } else if (length() < (1 << 16)) {
      m_value <<= 16;
      m_length <<= 16;
      uint16_t new_2_bytes;
      read_bytes(std::span<std::byte, 2>(reinterpret_cast<std::array<std::byte, 2>&>(new_2_bytes)));
      m_value |= std::rotl(new_2_bytes, 8);
    } else if (length() < (1 << 24)) {
      m_value <<= 8;
      m_length <<= 8;
      m_value |= static_cast<uint8_t>(read_byte());
    }
    return m_value;
  }
};

#else
class InStream : StreamVariables {
  constexpr static size_t BUFFER_SIZE = 256;
  std::istream& m_stream;
  std::array<uint32_t, BUFFER_SIZE> m_buffer;
  uint64_t current_big_chunk;
  uint32_t num_bytes_valid;
  uint32_t buffer_idx;
  int64_t valid_elements = std::numeric_limits<uint32_t>::max();

  uint32_t read_chunk() {
    if (buffer_idx == BUFFER_SIZE) {
      m_stream.read(reinterpret_cast<char*>(m_buffer.data()), sizeof(m_buffer));
      if (!m_stream) {
        valid_elements = (m_stream.gcount() + 3) / 4;
        m_stream.clear();
      }
      buffer_idx = 0;
    }
    LASPP_ASSERT_GE(valid_elements, buffer_idx);
    return m_buffer[buffer_idx++];
  }

  static_assert(std::endian::native == std::endian::little);

  void update_chunk() {
    if (num_bytes_valid < 4) {
      uint64_t new_chunk = read_chunk();
      current_big_chunk |= (new_chunk << (num_bytes_valid * 8));
      num_bytes_valid += 4;
    }
  }

  std::byte read_byte() {
    update_chunk();
    uint8_t byte = static_cast<uint8_t>(current_big_chunk);
    current_big_chunk >>= 8;
    num_bytes_valid--;
    return static_cast<std::byte>(byte);
  }

  template <uint32_t NumBytes>
  uint32_t read_bytes() {
    static_assert(NumBytes > 0 && NumBytes < 4);
    update_chunk();
    uint32_t new_bytes = current_big_chunk & ((1u << (NumBytes * 8)) - 1);
    current_big_chunk >>= NumBytes * 8;
    num_bytes_valid -= NumBytes;
    if constexpr (NumBytes == 2) {
      std::array<uint8_t, 4>& new_bytes_32 = reinterpret_cast<std::array<uint8_t, 4>&>(new_bytes);
      std::swap(new_bytes_32[0], new_bytes_32[1]);
    } else if constexpr (NumBytes == 3) {
      std::array<uint8_t, 4>& new_bytes_32 = reinterpret_cast<std::array<uint8_t, 4>&>(new_bytes);
      std::swap(new_bytes_32[0], new_bytes_32[2]);
    }
    return new_bytes;
  }

 public:
  explicit InStream(std::istream& stream)
      : m_stream(stream),
        m_buffer(),
        current_big_chunk(0),
        num_bytes_valid(0),
        buffer_idx(BUFFER_SIZE) {
    for (int i = 0; i < 4; i++) {
      m_value <<= 8;
      m_value |= static_cast<uint8_t>(read_byte());
    }
    m_length = std::numeric_limits<uint32_t>::max();
  }

  uint32_t length() const { return m_length; }

  void update_range(uint32_t lower, uint32_t upper) {
    m_value -= lower;
    m_length = upper - lower;
  }

  uint32_t get_value() {
    if (length() < (1 << 8)) {
      m_value <<= 24;
      m_length <<= 24;
      m_value |= read_bytes<3>();
    } else if (length() < (1 << 16)) {
      m_value <<= 16;
      m_length <<= 16;
      m_value |= read_bytes<2>();
    } else if (length() < (1 << 24)) {
      m_value <<= 8;
      m_length <<= 8;
      m_value |= static_cast<uint8_t>(read_byte());
    }
    return m_value;
  }

  std::istream& stream() { return m_stream; }
};
#endif

class OutStream : StreamVariables {
  std::iostream& m_stream;

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
  explicit OutStream(std::iostream& stream) : m_stream(stream) {
    m_base = 0;
    m_length = std::numeric_limits<uint32_t>::max();
  }

  ~OutStream() { finalize_stream(); }

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
