#pragma once

#include <array>
#include <iostream>
#include <optional>
#include <set>
#include <vector>

namespace laspp {

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
  os << "[";
  for (size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i < vec.size() - 1) {
      os << ", ";
    }
  }
  return os << "]";
}

template <typename T, size_t N>
inline std::ostream& operator<<(std::ostream& os, const std::array<T, N>& vec) {
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

}  // namespace laspp
