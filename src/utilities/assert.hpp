/*
 * Copyright (C) 2024 Trailblaze Software
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
 * with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For closed source licensing or development requests, contact
 * trailblaze.software@gmail.com
 */

#pragma once

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

#include <sstream>
#include <string>
#endif

namespace laspp {

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

#define LASPP_ASSERT(condition, ...) \
  if (!(condition)) laspp::_LASPP_FAIL_ASSERT(#condition, laspp::OptionalString(__VA_ARGS__));

inline void _LASPP_FAIL_ASSERT(const std::string &condition_str,
                               const std::optional<std::string> &message,
                               const std::source_location &loc = std::source_location::current()) {
  std::stringstream ss;
  ss << "Blaze assertion failed: " << condition_str << (message ? " " + *message : "") << "\n in "
     << loc.function_name() << " at " << loc.file_name() << ":" << loc.line() << std::endl;
  std::cerr << ss.str();
  throw std::runtime_error(ss.str());
}

#define LASPP_ASSERT_BIN_OP(a, b, op, nop)                                  \
  {                                                                         \
    const auto A = a;                                                       \
    const auto B = b;                                                       \
    if (!((A)op(B))) laspp::_LASPP_FAILBinOp((A), (B), (#a), (#b), (#nop)); \
  }

template <typename A, typename B>
inline void _LASPP_FAILBinOp(const A &a, const B &b, const std::string &a_str,
                             const std::string &b_str, const std::string &nop,
                             const std::source_location &loc = std::source_location::current()) {
  std::stringstream ss;
  ss << a << " " << nop << " " << b;
  laspp::_LASPP_FAIL_ASSERT(a_str + " " + nop + " " + b_str, ss.str(), loc);
}

#else
#define LASPP_ASSERT(condition, ...)
#define LASPP_ASSERT_BIN_OP(a, b, op, nop)
#endif

#define LASPP_FAIL(...)             \
  LASPP_ASSERT(false, __VA_ARGS__); \
  UNREACHABLE()  // LCOV_EXCL_LINE

#define LASPP_UNIMPLEMENTED(...) LASPP_FAIL("LASPP_UNIMPLEMENTED")

#define LASPP_ASSERT_GE(expr, val) LASPP_ASSERT_BIN_OP(expr, val, >=, <)
#define LASPP_ASSERT_LE(expr, val) LASPP_ASSERT_BIN_OP(expr, val, <=, >)
#define LASPP_ASSERT_GT(expr, val) LASPP_ASSERT_BIN_OP(expr, val, >, <=)
#define LASPP_ASSERT_EQ(expr, val) LASPP_ASSERT_BIN_OP(expr, val, ==, !=)
#define LASPP_ASSERT_NE(expr, val) LASPP_ASSERT_BIN_OP(expr, val, !=, ==)

#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
#define UNREACHABLE() __assume(false)
#else  // GCC, Clang
#define UNREACHABLE() __builtin_unreachable()
#endif

class RawString {
  std::vector<char> m_str;

 public:
  RawString(std::string str) : m_str(str.size()) {
    for (size_t i = 0; i < str.size(); i++) {
      m_str[i] = str[i];
    }
  }

  RawString(std::stringstream &ss) {
    for (char c : ss.str()) {
      m_str.push_back(c);
    }
  }

  RawString(std::initializer_list<uint8_t> vec) : m_str(vec.size()) {
    size_t i = 0;
    for (auto c : vec) {
      m_str[i++] = c;
    }
  }

  bool operator==(const RawString &other) const { return m_str == other.m_str; }

  friend std::ostream &operator<<(std::ostream &os, const RawString &rs) {
    os << "\"";
    for (size_t i = 0; i < rs.m_str.size(); i++) {
      os << (uint32_t)(uint8_t)rs.m_str[i];
      if (i < rs.m_str.size() - 1) {
        os << ", ";
      }
    }
    return os << "\"";
  }
};

#define DEBRACKET(X) DEBRACKET_(DEBRACKET_2 X)
#define DEBRACKET_2(...) DEBRACKET_2 __VA_ARGS__
#define DEBRACKET_(...) DEBRACKET_3(__VA_ARGS__)
#define DEBRACKET_3(...) VAN##__VA_ARGS__
#define VANDEBRACKET_2

#define LASPP_ASSERT_RAW_STR_EQ(expr, val) \
  LASPP_ASSERT_EQ(laspp::RawString(DEBRACKET(expr)), laspp::RawString(DEBRACKET(val)))

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

}  // namespace laspp
