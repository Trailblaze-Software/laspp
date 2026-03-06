/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
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

  void encode(OutStream& out_stream, std::byte byte) {
    uint8_t diff =
        static_cast<uint8_t>(static_cast<uint8_t>(byte) - static_cast<uint8_t>(m_previous_byte));
    m_symbol_encoder.encode_symbol(out_stream, diff);
    m_previous_byte = byte;
  }

  std::byte decode(InStream& in_stream) {
    uint8_t diff = static_cast<uint8_t>(m_symbol_encoder.decode_symbol(in_stream));
    m_previous_byte = static_cast<std::byte>(static_cast<uint8_t>(m_previous_byte) + diff);
    return m_previous_byte;
  }
};

class BytesEncoder {
  std::vector<ByteEncoder> m_byte_encoders;
  std::vector<std::byte> m_previous_bytes;

 public:
  using EncodedType = std::vector<std::byte>;

  explicit BytesEncoder(const std::vector<std::byte>& initial_bytes) {
    m_byte_encoders.reserve(initial_bytes.size());
    for (const auto& byte : initial_bytes) {
      m_byte_encoders.emplace_back(byte);
    }
    m_previous_bytes = initial_bytes;
  }

  void encode(OutStream& out_stream, const std::vector<std::byte>& bytes) {
    for (size_t i = 0; i < m_byte_encoders.size(); i++) {
      m_byte_encoders[i].encode(out_stream, bytes[i]);
    }
  }

  const std::vector<std::byte>& decode(InStream& in_stream) {
    for (size_t i = 0; i < m_byte_encoders.size(); i++) {
      m_previous_bytes[i] = m_byte_encoders[i].decode(in_stream);
    }
    return m_previous_bytes;
  }

  const std::vector<std::byte>& last_value() const { return m_previous_bytes; }
};

}  // namespace laspp
