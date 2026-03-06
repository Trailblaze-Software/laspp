/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include "laz/stream.hpp"

using namespace laspp;

#define CHECK_BASE_LENGTH_STRING(expected_base, expected_length, expected_string) \
  LASPP_ASSERT_EQ(out_stream.get_base(), expected_base);                          \
  LASPP_ASSERT_EQ(out_stream.length(), expected_length);                          \
  LASPP_ASSERT_RAW_STR_EQ(ss, expected_string);

int main() {
  std::stringstream ss;

  {
    OutStream out_stream(ss);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), std::numeric_limits<uint32_t>::max());

    out_stream.update_range(0, 1u << 31);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_EQ(ss.str(), "");

    out_stream.update_range(10, 20);
    CHECK_BASE_LENGTH_STRING(10u << 24, 10u << 24, ({0, 0, 0}));

    out_stream.update_range((10 << 24) - 1, (10 << 24));
    CHECK_BASE_LENGTH_STRING(0xff000000, 1u << 24, ({0, 0, 0, 19, 255, 255}));

    out_stream.update_range((1 << 24) - (1 << 16), 1 << 24);
    CHECK_BASE_LENGTH_STRING(0xff000000, 1u << 24, ({0, 0, 0, 19, 255, 255, 255}));

    out_stream.update_range(1 << 23, 1 << 24);
    CHECK_BASE_LENGTH_STRING(0x80000000, 1u << 31, ({0, 0, 0, 19, 255, 255, 255, 255}));

    out_stream.update_range(0x4000FFFF, 0x80000000);
    CHECK_BASE_LENGTH_STRING(0xC000FFFF, (1u << 31) - 0x4000FFFF,
                             ({0, 0, 0, 19, 255, 255, 255, 255}));

    out_stream.update_range(0, 1 << 7);
    CHECK_BASE_LENGTH_STRING(0xFF000000, 1u << 31,
                             ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 0, 0xFF}));

    out_stream.update_range(1 << 24, 1 << 25);
    CHECK_BASE_LENGTH_STRING(0, 1u << 24, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0}));

    out_stream.update_range(1 << 23, 1 << 24);
    CHECK_BASE_LENGTH_STRING(0x80000000, 1u << 31,
                             ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0}));

    out_stream.update_range(0x40FFFFFF, 0x80000000);
    CHECK_BASE_LENGTH_STRING(0xC0FFFFFF, (1u << 31) - 0x40FFFFFF,
                             ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0}));

    out_stream.update_range(0, 1 << 7);
    CHECK_BASE_LENGTH_STRING(0xFF000000, 1u << 31,
                             ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC0, 255, 255}));

    out_stream.update_range(1 << 24, 1 << 25);
    CHECK_BASE_LENGTH_STRING(0, 1u << 24,
                             ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC1, 0, 0}));

    out_stream.update_range((1 << 24) - 1, 1 << 24);
    CHECK_BASE_LENGTH_STRING(
        0xFF000000, 1u << 24,
        ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC1, 0, 0, 0, 255, 255}));
  }

  LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0,    0, 19, 255, 255, 255, 255, 0xC0, 1, 0,
                                0, 0xC1, 0, 0,  0,   255, 255, 255, 128,  0, 0}));
  LASPP_ASSERT_EQ(ss.tellg(), 0);

  return 0;
}
