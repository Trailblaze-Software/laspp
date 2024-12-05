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
    for (auto& byte : initial_bytes) {
      m_byte_encoders.push_back(ByteEncoder(byte));
    }
  }

  std::vector<std::byte> decode(InStream& in_stream) {
    std::vector<std::byte> bytes;
    for (auto& byte_encoder : m_byte_encoders) {
      bytes.push_back(byte_encoder.decode(in_stream));
    }
    return bytes;
  }
};

}  // namespace laspp
