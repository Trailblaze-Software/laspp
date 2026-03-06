/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#ifndef _MSC_VER
#define LASPP_HAS_BUILTIN(x) __has_builtin(x)
#else
#define LASPP_HAS_BUILTIN(x) 0
#endif

#ifdef LASPP_DEBUG_ASSERTS
#include <iostream>
#include <optional>
#if defined(_MSC_VER) || LASPP_HAS_BUILTIN(__builtin_source_location)
#include <source_location>
#else
#include <experimental/source_location>
namespace std {
using source_location = std::experimental::source_location;
}
#endif

#include "printing.hpp"
#endif

namespace laspp {

// Helper to conditionally cast to size_t (avoids useless-cast warnings)
template <typename T>
inline size_t to_size_t(T value) {
  if constexpr (std::is_same_v<T, size_t>) {
    return value;
  } else {
    return static_cast<size_t>(value);
  }
}

#ifdef LASPP_DEBUG_ASSERTS
template <typename... Args>
std::optional<std::string> OptionalString(Args &&...args) {
  if constexpr (sizeof...(args) == 0) {
    return std::nullopt;
  } else {
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
  }
}

#define LASPP_ASSERT(condition, ...)                                          \
  if (!(condition))                                                           \
    laspp::_LASPP_FAIL_ASSERT(#condition, laspp::OptionalString(__VA_ARGS__), \
                              std::source_location::current());

inline void _LASPP_FAIL_ASSERT(const std::string &condition_str,
                               const std::optional<std::string> &message,
                               const std::source_location &loc = std::source_location::current()) {
  std::stringstream ss;
  std::string_view s = loc.file_name();
  std::string_view filename = s.substr(s.find_last_of('/') + 1);
  ss << "LAS++ assertion failed: " << condition_str << (message ? " " + *message : "") << "\n in "
     << loc.function_name() << " at " << filename << ":" << loc.line() << std::endl;
  std::cerr << ss.str();
  throw std::runtime_error(ss.str());
}

#ifdef _MSC_VER
#define LASPP_ASSERT_BIN_OP(a, b, op, nop, ...)                                            \
  {                                                                                        \
    const auto A = a;                                                                      \
    const auto B = b;                                                                      \
    __pragma(warning(push)) __pragma(warning(disable : 4127)) if (!((A)op(B)))             \
        __pragma(warning(pop)) laspp::_LASPP_FAILBinOp((A), (B), (#a), (#b), (#nop),       \
                                                       laspp::OptionalString(__VA_ARGS__), \
                                                       std::source_location::current());   \
  }
#else
#define LASPP_ASSERT_BIN_OP(a, b, op, nop, ...)                                                 \
  {                                                                                             \
    const auto A = a;                                                                           \
    const auto B = b;                                                                           \
    if (!((A)op(B)))                                                                            \
      laspp::_LASPP_FAILBinOp((A), (B), (#a), (#b), (#nop), laspp::OptionalString(__VA_ARGS__), \
                              std::source_location::current());                                 \
  }
#endif

template <typename A, typename B>
inline void _LASPP_FAILBinOp(const A &a, const B &b, const std::string &a_str,
                             const std::string &b_str, const std::string &nop,
                             const std::optional<std::string> &message,
                             const std::source_location &loc = std::source_location::current()) {
  std::stringstream ss;
  ss << a << " " << nop << " " << b;
  laspp::_LASPP_FAIL_ASSERT(a_str + " " + nop + " " + b_str,
                            message ? (ss.str() + " " + *message) : ss.str(), loc);
}

#else
#define LASPP_ASSERT(condition, ...)
#define LASPP_ASSERT_BIN_OP(a, b, op, nop, ...)
#endif

#define LASPP_FAIL(...)             \
  LASPP_ASSERT(false, __VA_ARGS__); \
  UNREACHABLE()  // LCOV_EXCL_LINE

#define LASPP_UNIMPLEMENTED(...) LASPP_FAIL("LASPP_UNIMPLEMENTED")

#define LASPP_ASSERT_GE(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, >=, <, __VA_ARGS__)
#define LASPP_ASSERT_LE(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, <=, >, __VA_ARGS__)
#define LASPP_ASSERT_GT(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, >, <=, __VA_ARGS__)
#define LASPP_ASSERT_LT(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, <, >=, __VA_ARGS__)
#define LASPP_ASSERT_EQ(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, ==, !=, __VA_ARGS__)
#define LASPP_ASSERT_NE(expr, val, ...) LASPP_ASSERT_BIN_OP(expr, val, !=, ==, __VA_ARGS__)

#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
#define UNREACHABLE() __assume(false)
#else  // GCC, Clang
#define UNREACHABLE() __builtin_unreachable()
#endif

class RawString {
  std::vector<uint8_t> m_str;

 public:
  explicit RawString(const std::string &str) : m_str(str.size()) {
    for (size_t i = 0; i < str.size(); i++) {
      m_str[i] = static_cast<uint8_t>(str[i]);
    }
  }

  explicit RawString(const std::stringstream &ss) {
    for (char c : ss.str()) {
      m_str.push_back(static_cast<uint8_t>(c));
    }
  }

  explicit RawString(std::initializer_list<uint8_t> vec) : m_str(vec.size()) {
    size_t i = 0;
    for (uint8_t c : vec) {
      m_str[i++] = c;
    }
  }

  bool operator==(const RawString &other) const { return m_str == other.m_str; }

  friend std::ostream &operator<<(std::ostream &os, const RawString &rs) {
    os << "\"";
    for (size_t i = 0; i < rs.m_str.size(); i++) {
      os << static_cast<uint32_t>(rs.m_str[i]);
      if (i < rs.m_str.size() - 1) {
        os << ", ";
      }
    }
    return os << "\"";
  }
};

#define DEBRACKET(X) DEBRACKET_1(BRACKET X)
#define BRACKET(...) BRACKET __VA_ARGS__
#define DEBRACKET_1(...) DEBRACKET_0(__VA_ARGS__)
#define DEBRACKET_0(...) NO##__VA_ARGS__
#define NOBRACKET

constexpr int32_t f_arr([[maybe_unused]] std::array<int, 2> arr) { return 1; }
static_assert(f_arr(std::array<int, 2>{{4, 4}}));
static_assert(f_arr(DEBRACKET((std::array<int, 2>{{4, 4}}))));

#define LASPP_ASSERT_RAW_STR_EQ(expr, val) \
  LASPP_ASSERT_EQ(laspp::RawString(DEBRACKET(expr)), laspp::RawString(DEBRACKET(val)))

#ifdef LASPP_DEBUG_ASSERTS
#define LASPP_ASSERT_THROWS(expr, exception)               \
  {                                                        \
    bool caught = false;                                   \
    try {                                                  \
      DEBRACKET(expr);                                     \
    } catch (const exception &) {                          \
      caught = true;                                       \
    }                                                      \
    LASPP_ASSERT(caught, "Expected exception " #exception) \
  }
#else
#define LASPP_ASSERT_THROWS(expr, exception)
#endif

// LASPP_CHECK_READ checks for read errors and byte count, even when debug asserts are off
// Parameters: stream, buffer, size
#define LASPP_CHECK_READ(stream, buffer, size)                                                 \
  {                                                                                            \
    auto &laspp_check_stream = (stream);                                                       \
    size_t laspp_expected_size = laspp::to_size_t(size);                                       \
    laspp_check_stream.read(reinterpret_cast<char *>(buffer),                                  \
                            static_cast<std::streamsize>(laspp_expected_size));                \
    std::streamsize laspp_bytes_read = laspp_check_stream.gcount();                            \
    if (!laspp_check_stream || static_cast<size_t>(laspp_bytes_read) != laspp_expected_size) { \
      std::stringstream laspp_error_msg;                                                       \
      laspp_error_msg << "Failed to read from stream: expected " << laspp_expected_size        \
                      << " bytes, got " << laspp_bytes_read << " bytes";                       \
      throw std::runtime_error(laspp_error_msg.str());                                         \
    }                                                                                          \
  }

// Macros to suppress useless-cast warnings only for GCC/Clang
#if defined(__GNUC__) || defined(__clang__)
#define LASPP_PUSH_WARNING_DISABLE_USELESS_CAST \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define LASPP_POP_WARNING _Pragma("GCC diagnostic pop")
#else
#define LASPP_PUSH_WARNING_DISABLE_USELESS_CAST
#define LASPP_POP_WARNING
#endif

// LASPP_CHECK_SEEK checks if a seek operation succeeded
// Parameters: stream, offset, direction (e.g., std::ios::beg, std::ios::cur, std::ios::end)
// Note: Cast to std::streamoff is needed for sign conversion (size_t -> signed) and is
//       harmless when types already match (may trigger -Wuseless-cast in some cases)
#define LASPP_CHECK_SEEK(stream, offset, direction)                           \
  {                                                                           \
    auto &laspp_check_stream = (stream);                                      \
    LASPP_PUSH_WARNING_DISABLE_USELESS_CAST                                   \
    laspp_check_stream.seekg(static_cast<std::streamoff>(offset), direction); \
    LASPP_POP_WARNING                                                         \
    if (!laspp_check_stream.good()) {                                         \
      std::stringstream laspp_error_msg;                                      \
      laspp_error_msg << "Failed to seek in stream to offset " << offset;     \
      throw std::runtime_error(laspp_error_msg.str());                        \
    }                                                                         \
  }

}  // namespace laspp
