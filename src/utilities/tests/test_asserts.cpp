/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include "utilities/assert.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  LASPP_ASSERT_EQ(1, 1);
  LASPP_ASSERT(true);

  LASPP_ASSERT_THROWS({ LASPP_ASSERT_EQ(1, 2); }, std::runtime_error);
  LASPP_ASSERT_THROWS({ LASPP_ASSERT(false, "Amazing"); }, std::runtime_error);
  LASPP_ASSERT_THROWS({ LASPP_ASSERT_RAW_STR_EQ("hello", "world"); }, std::runtime_error);

  return 0;
}
