/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
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

  test_values.emplace_back(std::vector<int>{-1, -2, -3, -4, -5, -6, -7, -8, -9, -10});
  expected_values.emplace_back(std::vector<int>{0, 0, -1, -2, -3, -4, -5, -6, -7, -8});
  test_values.emplace_back(std::vector<int>{-1, -2, -3, -4, -5, -6, -7, 8, 9, 10});
  expected_values.emplace_back(std::vector<int>{0, 0, -1, -2, -3, -4, -5, -5, -4, 8});

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
