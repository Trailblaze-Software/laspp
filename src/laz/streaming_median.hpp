
/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <ostream>

#include "utilities/printing.hpp"

namespace laspp {

template <typename T>
class StreamingMedian {
  std::array<T, 5> m_vals;
  bool m_remove_largest;

 public:
  StreamingMedian() : m_vals{}, m_remove_largest(true) {}

  void insert(const T val) {
    bool next_remove_largest;
    if (val > get_median()) {
      next_remove_largest = false;
    } else if (val < get_median()) {
      next_remove_largest = true;
    } else {
      next_remove_largest = !m_remove_largest;
    }
    if (m_remove_largest) {
      m_vals[4] = val;
      size_t idx = 4;
      while (idx > 0 && m_vals[idx] < m_vals[idx - 1]) {
        std::swap(m_vals[idx - 1], m_vals[idx]);
        idx--;
      }
    } else {
      m_vals[0] = val;
      size_t idx = 0;
      while (idx < 4 && m_vals[idx] > m_vals[idx + 1]) {
        std::swap(m_vals[idx + 1], m_vals[idx]);
        idx++;
      }
    }
    m_remove_largest = next_remove_largest;
  }

  T get_median() { return m_vals[2]; }

  const std::array<T, 5>& get_vals() const { return m_vals; }

  friend std::ostream& operator<<(std::ostream& os, const StreamingMedian& sm) {
    return os << sm.m_vals << " " << sm.m_remove_largest;
  }
};
}  // namespace laspp
