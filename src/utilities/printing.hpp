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
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace laspp {

inline std::ostream& operator<<(std::ostream& os, const std::byte& b) {
  return os << static_cast<uint32_t>(b);
};

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::span<T>& vec) {
  os << "[";
  for (size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i < vec.size() - 1) {
      os << ", ";
    }
  }
  return os << "]";
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
  return os << std::span<const T>(vec);
}

template <typename T, size_t N>
inline std::ostream& operator<<(std::ostream& os, const std::array<T, N>& arr) {
  os << "[";
  for (size_t i = 0; i < arr.size(); i++) {
    os << arr[i];
    if (i < arr.size() - 1) {
      os << ", ";
    }
  }
  return os << "]";
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::set<T>& set) {
  os << "{";
  for (const T& elem : set) {
    os << elem << ", ";
  }
  return os << "}";
}

template <typename T, typename U>
inline std::ostream& operator<<(std::ostream& os, const std::pair<T, U>& pair) {
  return os << "(" << pair.first << ", " << pair.second << ")";
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::optional<T>& op) {
  if (op.has_value()) {
    return os << "Some(" << op.value() << ")";
  }
  return os << "None";
}

template <typename... Args>
inline std::ostream& operator<<(std::ostream& os, const std::tuple<Args...>& tup) {
  std::apply([&os](const Args&... args) { ((os << args << ", "), ...); }, tup);
  return os;
}

template <typename K, typename V>
inline std::ostream& operator<<(std::ostream& os, const std::map<K, V>& map) {
  os << "{";
  for (const auto& [k, v] : map) {
    os << k << ": " << v << ", ";
  }
  return os << "}";
}

/**
 * Wrapper class to limit the number of items printed from a map
 * Usage: std::cout << limited_map(map, 5) << std::endl;
 */
template <typename K, typename V>
class LimitedMap {
  const std::map<K, V>& m_map;
  size_t m_limit;
  bool m_show_remaining;

 public:
  LimitedMap(const std::map<K, V>& map, size_t limit, bool show_remaining = true)
      : m_map(map), m_limit(limit), m_show_remaining(show_remaining) {}

  friend std::ostream& operator<<(std::ostream& os, const LimitedMap<K, V>& limited) {
    size_t count = 0;
    for (const auto& [k, v] : limited.m_map) {
      if (count++ >= limited.m_limit) {
        break;
      }
      os << v << std::endl;
    }
    if (limited.m_show_remaining && limited.m_map.size() > limited.m_limit) {
      os << "... (" << (limited.m_map.size() - limited.m_limit) << " more)" << std::endl;
    }
    return os;
  }
};

/**
 * Helper function to create a LimitedMap wrapper
 * Usage: std::cout << indented(limited_map(map, 5), "  ") << std::endl;
 */
template <typename K, typename V>
inline LimitedMap<K, V> limited_map(const std::map<K, V>& map, size_t limit,
                                    bool show_remaining = true) {
  return LimitedMap<K, V>(map, limit, show_remaining);
}

/**
 * Wrapper class to add indentation prefix to each line of output from operator<<
 * Usage: std::cout << indented(vlr, "  ") << std::endl;
 */
template <typename T>
class Indented {
  const T& m_value;
  std::string m_indent;

 public:
  Indented(const T& value, const std::string& indent) : m_value(value), m_indent(indent) {}

  friend std::ostream& operator<<(std::ostream& os, const Indented<T>& indented) {
    // Capture the output from the object's operator<<
    std::ostringstream ss;
    ss << indented.m_value;
    std::string output = ss.str();

    // Add indentation to each line
    if (output.empty()) {
      return os;
    }

    size_t start = 0;
    while (start < output.length()) {
      size_t end = output.find('\n', start);
      if (end == std::string::npos) {
        // Last line (no trailing newline)
        if (start < output.length()) {
          os << indented.m_indent;
          os.write(output.data() + start, static_cast<std::streamsize>(output.length() - start));
        }
        break;
      }

      // Add indentation prefix to this line
      os << indented.m_indent;
      os.write(output.data() + start, static_cast<std::streamsize>(end - start));
      os << '\n';

      start = end + 1;
    }

    return os;
  }
};

/**
 * Helper function to create an Indented wrapper
 * Usage: std::cout << indented(vlr, "  ") << std::endl;
 */
template <typename T>
inline Indented<T> indented(const T& value, const std::string& indent) {
  return Indented<T>(value, indent);
}

}  // namespace laspp
