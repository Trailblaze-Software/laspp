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

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "utilities/assert.hpp"
#include "utilities/printing.hpp"

using namespace laspp;

// Simple test class with operator<<
struct TestStruct {
  int value;
  friend std::ostream& operator<<(std::ostream& os, const TestStruct& ts) {
    return os << "TestStruct(" << ts.value << ")";
  }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test Indented with simple string output
  {
    std::ostringstream oss;
    std::string test_str = "line1\nline2\nline3";
    oss << test_str;
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "line1\nline2\nline3");
  }

  // Test Indented with single line
  {
    std::ostringstream oss;
    TestStruct ts{42};
    oss << indented(ts, "  ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "  TestStruct(42)");
  }

  // Test Indented with multiple lines
  {
    std::ostringstream oss;
    std::string multi_line = "first\nsecond\nthird";
    oss << indented(multi_line, "  ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "  first\n  second\n  third");
  }

  // Test Indented with empty string
  {
    std::ostringstream oss;
    std::string empty = "";
    oss << indented(empty, "  ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "");
  }

  // Test Indented with trailing newline
  {
    std::ostringstream oss;
    std::string with_newline = "line1\nline2\n";
    oss << indented(with_newline, "  ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "  line1\n  line2\n");
  }

  // Test Indented with custom indent
  {
    std::ostringstream oss;
    std::string text = "a\nb\nc";
    oss << indented(text, "    ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "    a\n    b\n    c");
  }

  // Test Indented with vector
  {
    std::ostringstream oss;
    std::vector<int> vec = {1, 2, 3};
    oss << indented(vec, "  ");
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "  [1, 2, 3]");
  }

  // Test LimitedMap with small map
  {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}, {3, "three"}};
    std::ostringstream oss;
    oss << limited_map(map, 2);
    std::string result = oss.str();
    LASPP_ASSERT(result.find("one") != std::string::npos);
    LASPP_ASSERT(result.find("two") != std::string::npos);
    LASPP_ASSERT(result.find("three") == std::string::npos);  // Should be limited
    LASPP_ASSERT(result.find("more") != std::string::npos);   // Should show remaining
  }

  // Test LimitedMap with limit larger than map size
  {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}};
    std::ostringstream oss;
    oss << limited_map(map, 10);
    std::string result = oss.str();
    LASPP_ASSERT(result.find("one") != std::string::npos);
    LASPP_ASSERT(result.find("two") != std::string::npos);
    LASPP_ASSERT(result.find("more") == std::string::npos);  // Should not show remaining
  }

  // Test LimitedMap with show_remaining = false
  {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}, {3, "three"}};
    std::ostringstream oss;
    oss << limited_map(map, 1, false);
    std::string result = oss.str();
    LASPP_ASSERT(result.find("one") != std::string::npos);
    LASPP_ASSERT(result.find("more") == std::string::npos);  // Should not show remaining
  }

  // Test LimitedMap with empty map
  {
    std::map<int, std::string> map;
    std::ostringstream oss;
    oss << limited_map(map, 5);
    std::string result = oss.str();
    LASPP_ASSERT_EQ(result, "");
  }

  // Test LimitedMap with limit of 0
  {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}};
    std::ostringstream oss;
    oss << limited_map(map, 0);
    std::string result = oss.str();
    LASPP_ASSERT(result.find("one") == std::string::npos);
    LASPP_ASSERT(result.find("more") != std::string::npos);  // Should show remaining
  }

  // Test combination: indented limited_map
  {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}, {3, "three"}};
    std::ostringstream oss;
    oss << indented(limited_map(map, 2), "    ");
    std::string result = oss.str();
    LASPP_ASSERT(result.find("    one") != std::string::npos);
    LASPP_ASSERT(result.find("    two") != std::string::npos);
    LASPP_ASSERT(result.find("    ...") != std::string::npos);
  }

  return 0;
}
