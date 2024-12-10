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

 public:
  BytesEncoder(std::vector<std::byte> initial_bytes) {
    m_byte_encoders.reserve(initial_bytes.size());
    for (const auto& byte : initial_bytes) {
      m_byte_encoders.emplace_back(byte);
    }
  }

  void decode(InStream& in_stream, std::vector<std::byte>& out_bytes) {
    out_bytes.resize(std::max(out_bytes.size(), m_byte_encoders.size()));
    for (size_t i = 0; i < m_byte_encoders.size(); i++) {
      out_bytes[i] = m_byte_encoders[i].decode(in_stream);
    }
  }
};

}  // namespace laspp
