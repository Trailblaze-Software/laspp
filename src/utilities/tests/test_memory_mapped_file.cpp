/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "utilities/assert.hpp"
#include "utilities/memory_mapped_file.hpp"

using namespace laspp::utilities;

class TempFile {
 public:
  explicit TempFile(const std::string& prefix) {
    auto base_dir = std::filesystem::temp_directory_path() / "laspp_mmap_tests";
    std::filesystem::create_directories(base_dir);
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = base_dir / (prefix + "_" + std::to_string(timestamp) + ".tmp");
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // Test 1: Non-existent file (tests m_fd == -1 / INVALID_HANDLE_VALUE path)
  {
    std::filesystem::path non_existent =
        std::filesystem::temp_directory_path() / "laspp_mmap_tests" / "does_not_exist.tmp";
    bool threw_correctly = false;
    std::string error_message;
    try {
      MemoryMappedFile mmap(non_existent.string());
    } catch (const std::runtime_error& e) {
      threw_correctly = true;
      error_message = e.what();
      // Verify error message contains expected text
      LASPP_ASSERT(
          error_message.find("Failed to open") != std::string::npos ||
              error_message.find("Failed to open file for memory mapping") != std::string::npos,
          "Error message should mention file open failure");
    }
    LASPP_ASSERT(threw_correctly, "Should throw std::runtime_error for non-existent file");
  }

  // Test 2: Empty file (tests st.st_size == 0 / file_size.QuadPart == 0 path)
  {
    TempFile empty_file("empty");
    {
      // Create empty file
      std::ofstream ofs(empty_file.path(), std::ios::binary);
      ofs.close();
    }

    // Empty file should construct successfully but have size 0 and not be valid
    MemoryMappedFile mmap(empty_file.path().string());
    LASPP_ASSERT_EQ(mmap.size(), 0u, "Empty file should have size 0");
    LASPP_ASSERT(!mmap.is_valid(), "Empty file should not be valid");
    LASPP_ASSERT_EQ(mmap.data().size(), 0u, "Empty file data span should have size 0");
  }

  // Test 3: Valid file (success path - verify all operations work)
  {
    TempFile valid_file("valid");
    const char test_data[] = "Hello, World!";
    const size_t test_data_size = sizeof(test_data) - 1;
    {
      std::ofstream ofs(valid_file.path(), std::ios::binary);
      ofs.write(test_data, static_cast<std::streamsize>(test_data_size));
      ofs.close();
    }

    MemoryMappedFile mmap(valid_file.path().string());
    LASPP_ASSERT(mmap.is_valid(), "Valid file should be valid");
    LASPP_ASSERT_EQ(mmap.size(), test_data_size, "File size should match written data");
    LASPP_ASSERT_EQ(mmap.data().size(), mmap.size(), "Data span size should match file size");

    // Verify we can read the data correctly
    auto data_span = mmap.data();
    LASPP_ASSERT_EQ(data_span.size(), test_data_size, "Data span should have correct size");
    for (size_t i = 0; i < test_data_size; ++i) {
      LASPP_ASSERT_EQ(static_cast<char>(data_span[i]), test_data[i],
                      "Mapped data should match written data at index ", i);
    }
  }

  // Test 4: Move constructor and assignment (verify move semantics work correctly)
  {
    TempFile move_file("move_test");
    const char test_data[] = "test data";
    const size_t test_data_size = sizeof(test_data) - 1;
    {
      std::ofstream ofs(move_file.path(), std::ios::binary);
      ofs.write(test_data, static_cast<std::streamsize>(test_data_size));
      ofs.close();
    }

    MemoryMappedFile mmap1(move_file.path().string());
    size_t original_size = mmap1.size();
    LASPP_ASSERT_GT(original_size, 0u, "File should have non-zero size");

    // Move constructor - verify moved-to object works
    MemoryMappedFile mmap2(std::move(mmap1));
    LASPP_ASSERT_EQ(mmap2.size(), original_size, "Moved file should have same size");
    LASPP_ASSERT(mmap2.is_valid(), "Moved file should be valid");
    // Verify moved-from object is in valid empty state
    LASPP_ASSERT_EQ(mmap1.size(), 0u, "Moved-from file should have size 0");
    LASPP_ASSERT(!mmap1.is_valid(), "Moved-from file should not be valid");

    // Move assignment
    TempFile dummy_file("dummy");
    {
      std::ofstream ofs(dummy_file.path(), std::ios::binary);
      ofs.write("dummy", 5);
      ofs.close();
    }
    MemoryMappedFile mmap3(dummy_file.path().string());
    mmap3 = std::move(mmap2);
    // Verify move-assigned object has correct state
    LASPP_ASSERT_EQ(mmap3.size(), original_size, "Move-assigned file should have original size");
    LASPP_ASSERT(mmap3.is_valid(), "Move-assigned file should be valid");
    // Verify moved-from object is in valid empty state
    LASPP_ASSERT_EQ(mmap2.size(), 0u, "Move-assigned-from file should have size 0");
    LASPP_ASSERT(!mmap2.is_valid(), "Move-assigned-from file should not be valid");
  }

  // Test 5: subspan bounds checking and data access
  {
    TempFile span_file("span_test");
    const char data[] = "0123456789";
    const size_t data_size = sizeof(data) - 1;
    {
      std::ofstream ofs(span_file.path(), std::ios::binary);
      ofs.write(data, static_cast<std::streamsize>(data_size));
      ofs.close();
    }

    MemoryMappedFile mmap(span_file.path().string());
    LASPP_ASSERT_EQ(mmap.size(), data_size, "File size should match written data");

    // Valid subspan at start
    auto span1 = mmap.subspan(0, 5);
    LASPP_ASSERT_EQ(span1.size(), 5u, "Valid subspan should have correct size");
    LASPP_ASSERT_EQ(static_cast<char>(span1[0]), '0', "Subspan should contain correct data");
    LASPP_ASSERT_EQ(static_cast<char>(span1[4]), '4', "Subspan should contain correct data");

    // Valid subspan in middle
    auto span2 = mmap.subspan(3, 4);
    LASPP_ASSERT_EQ(span2.size(), 4u, "Middle subspan should have correct size");
    LASPP_ASSERT_EQ(static_cast<char>(span2[0]), '3', "Middle subspan should contain correct data");

    // Valid subspan at end
    auto span3 = mmap.subspan(5, 5);
    LASPP_ASSERT_EQ(span3.size(), 5u, "End subspan should have correct size");
    LASPP_ASSERT_EQ(static_cast<char>(span3[0]), '5', "End subspan should contain correct data");

    // Subspan that would exceed bounds (should be caught by assertion in debug mode)
#ifdef LASPP_DEBUG_ASSERTS
    LASPP_ASSERT_THROWS(
        {
          auto bad_span = mmap.subspan(0, mmap.size() + 1);
          (void)bad_span;  // Use the result to avoid [[nodiscard]] warning
        },
        std::runtime_error);
#endif
  }

  // Test 7: Verify data() returns correct span
  {
    TempFile data_file("data_test");
    const char test_data[] = "ABCDEF";
    const size_t test_data_size = sizeof(test_data) - 1;
    {
      std::ofstream ofs(data_file.path(), std::ios::binary);
      ofs.write(test_data, static_cast<std::streamsize>(test_data_size));
      ofs.close();
    }

    MemoryMappedFile mmap(data_file.path().string());
    auto data_span = mmap.data();
    LASPP_ASSERT_EQ(data_span.size(), mmap.size(), "data() span size should match file size");
    LASPP_ASSERT_EQ(data_span.size(), test_data_size, "data() span should have correct size");

    // Verify we can access all bytes
    for (size_t i = 0; i < test_data_size; ++i) {
      LASPP_ASSERT_EQ(static_cast<char>(data_span[i]), test_data[i],
                      "data() span should contain correct data at index ", i);
    }
  }

  return 0;
}
