/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#pragma once

#include <vector>

#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"

namespace laspp {

class ByteEncoder {
  std::byte m_previous_byte;
  SymbolEncoder<256> m_symbol_encoder;

 public:
  explicit ByteEncoder(std::byte initial_byte) : m_previous_byte(initial_byte) {}

  std::byte decode(InStream& in_stream) {
    uint8_t diff = static_cast<uint8_t>(m_symbol_encoder.decode_symbol(in_stream));
    m_previous_byte = static_cast<std::byte>(static_cast<uint8_t>(m_previous_byte) + diff);
    return m_previous_byte;
  }
};

class BytesEncoder {
  std::vector<ByteEncoder> m_byte_encoders;
  std::vector<std::byte> m_last_bytes;

 public:
  using EncodedType = std::vector<std::byte>;
  const EncodedType& last_value() const { return m_last_bytes; }

  explicit BytesEncoder(const std::vector<std::byte>& initial_bytes) {
    m_byte_encoders.reserve(initial_bytes.size());
    for (const auto& byte : initial_bytes) {
      m_byte_encoders.emplace_back(byte);
    }
    m_last_bytes = initial_bytes;
  }

  void decode(InStream& in_stream) {
    for (size_t i = 0; i < m_byte_encoders.size(); i++) {
      reinterpret_cast<uint8_t&>(m_last_bytes[i]) +=
          static_cast<uint8_t>(m_byte_encoders[i].decode(in_stream));
    }
  }
};

}  // namespace laspp
