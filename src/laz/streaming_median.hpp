
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

#include <array>
#include <cstdint>

namespace laspp {

template <typename T>
class StreamingMedian {
  std::array<T, 5> m_vals;
  bool m_remove_largest;

 public:
  StreamingMedian() : m_vals{0, 0, 0, 0, 0}, m_remove_largest(true) {}

  void insert(T val) {
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
      uint8_t idx = 4;
      while (idx > 0 && m_vals[idx] < m_vals[idx - 1]) {
        std::swap(m_vals[idx - 1], m_vals[idx]);
        idx--;
      }
    } else {
      m_vals[0] = val;
      uint8_t idx = 0;
      while (idx < 4 && m_vals[idx] > m_vals[idx + 1]) {
        std::swap(m_vals[idx + 1], m_vals[idx]);
        idx++;
      }
    }
    m_remove_largest = next_remove_largest;
  }

  T get_median() { return m_vals[2]; }
};
}  // namespace laspp
