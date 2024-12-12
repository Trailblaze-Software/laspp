#pragma once

#include <vector>

#include "laz/stream.hpp"
#include "laz/symbol_encoder.hpp"
#include "utilities/assert.hpp"
namespace laspp {

class ByteEncoder {
  std::byte m_previous_byte;
  SymbolEncoder<256> m_symbol_encoder;

 public:
  explicit ByteEncoder(std::byte initial_byte) : m_previous_byte(initial_byte) {}

  std::byte decode(InStream& in_stream) {
    uint8_t diff = m_symbol_encoder.decode_symbol(in_stream);
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

  BytesEncoder(const std::vector<std::byte>& initial_bytes) {
    m_byte_encoders.reserve(initial_bytes.size());
    for (const auto& byte : initial_bytes) {
      m_byte_encoders.emplace_back(byte);
    }
    m_last_bytes = initial_bytes;
  }

  void decode(InStream& in_stream) {
    for (size_t i = 0; i < m_byte_encoders.size(); i++) {
      (uint8_t&)m_last_bytes[i] += (uint8_t)m_byte_encoders[i].decode(in_stream);
    }
  }
};

}  // namespace laspp
