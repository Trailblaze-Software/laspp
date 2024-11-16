#pragma once

#include <cstdint>
#include <iostream>
#include <istream>
#include <limits>

namespace laspp {

struct StreamVariables {
  uint32_t m_base;
  uint32_t m_value;
  uint32_t m_length;
};

class InStream : StreamVariables {
  std::istream& m_stream;

 public:
  InStream(std::istream& stream) : m_stream(stream) {
    for (int i = 0; i < 4; i++) {
      m_value <<= 8;
      uint8_t byte;
      byte = m_stream.get();
      m_value += byte;
    }
    m_length = std::numeric_limits<uint32_t>::max();
  }

  uint32_t length() const { return m_length; }

  void update_range(uint32_t lower, uint32_t upper) {
    m_value -= lower;
    m_length = upper - lower;
  }

  uint32_t get_value() {
    while (length() < (1 << 24)) {
      m_value <<= 8;
      uint8_t new_byte;
      new_byte = m_stream.get();
      m_value += new_byte;
      m_length <<= 8;
    }

    return m_value;
  }
};

class OutStream : StreamVariables {
  std::ostream& m_stream;

  void finalize_stream() {
    bool write_two_bytes;
    if (length() > (1u << 25)) {
      update_range(1u << 24, 0b11u << 23);
      write_two_bytes = false;
    } else {
      update_range(1u << 23, (1u << 23) + (1u << 15));
      write_two_bytes = true;
    }
    get_base();
    get_base();
    m_stream.put(0);
    m_stream.put(0);
    if (!write_two_bytes) {
      m_stream.put(0);
    }
  }

 public:
  OutStream(std::ostream& stream) : m_stream(stream) {
    m_base = 0;
    m_length = std::numeric_limits<uint32_t>::max();
  }

  ~OutStream() { finalize_stream(); }

  uint32_t length() const { return m_length; }

  void propogate_carry() { std::cout << "NEED TO IMPLEMENT SOMEHOW" << std::endl; }

  void update_range(uint32_t lower, uint32_t upper) {
    if ((size_t)m_base + (size_t)lower >= 1ul << 32) {
      propogate_carry();
    }
    m_base += lower;
    m_length = upper - lower;
  }

  uint32_t get_base() {
    while (length() < (1 << 24)) {
      uint8_t to_write = m_base >> 24;
      m_stream.put(to_write);
      m_base <<= 8;
      m_length <<= 8;
    }
    return m_value;
  }
};
}  // namespace laspp
