/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#include <vector>

#include "laz/streaming_median.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main() {
  std::vector<std::vector<int>> test_values;
  std::vector<std::vector<int>> expected_values;
  test_values.emplace_back(std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  expected_values.emplace_back(std::vector<int>{0, 0, 1, 2, 3, 4, 5, 6, 7, 8});

  test_values.emplace_back(std::vector<int>{10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  expected_values.emplace_back(std::vector<int>{0, 0, 8, 8, 7, 6, 5, 4, 3, 2});

  test_values.emplace_back(std::vector<int>{3, 3, 3, 3, 3, 3, 3, 3, 3, 3});
  expected_values.emplace_back(std::vector<int>{0, 0, 3, 3, 3, 3, 3, 3, 3, 3});

  for (size_t test_idx = 0; test_idx < test_values.size(); test_idx++) {
    std::vector<int> values_to_insert = test_values[test_idx];
    std::vector<int> expected_medians = expected_values[test_idx];

    StreamingMedian<int> streaming_median;

    std::vector<int> result;
    for (size_t i = 0; i < values_to_insert.size(); i++) {
      streaming_median.insert(values_to_insert[i]);
      result.push_back(streaming_median.get_median());
    }

    LASPP_ASSERT_EQ(result, expected_medians);
  }

  return 0;
}
