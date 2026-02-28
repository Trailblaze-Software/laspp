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

#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "utilities/assert.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace laspp::utilities {

// Platform-agnostic memory-mapped file interface.
// Provides read-only access to file data via direct memory mapping.
class MemoryMappedFile {
 public:
  MemoryMappedFile(const MemoryMappedFile&) = delete;
  MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

  // Constructs a memory-mapped file. Throws on failure.
  explicit MemoryMappedFile(std::string_view file_path);

  // Unmaps the file.
  ~MemoryMappedFile();

  // Move constructor and assignment
  MemoryMappedFile(MemoryMappedFile&& other) noexcept;
  MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

  // Returns a span to the mapped memory. Valid until the object is destroyed.
  [[nodiscard]] std::span<const std::byte> data() const noexcept {
    return std::span<const std::byte>(m_data, m_size);
  }

  // Returns the size of the mapped file in bytes.
  [[nodiscard]] std::size_t size() const noexcept { return m_size; }

  // Returns true if the mapping is valid.
  [[nodiscard]] bool is_valid() const noexcept { return m_data != nullptr; }

  // Get a subspan of the mapped data. Bounds-checked.
  [[nodiscard]] std::span<const std::byte> subspan(std::size_t offset, std::size_t count) const {
    LASPP_ASSERT_LE(offset + count, m_size);
    return std::span<const std::byte>(m_data + offset, count);
  }

 private:
  const std::byte* m_data = nullptr;
  std::size_t m_size = 0;

#ifdef _WIN32
  void* m_file_handle = nullptr;
  void* m_mapping_handle = nullptr;
#else
  int m_fd = -1;
#endif

  void map_file(std::string_view file_path);
  void unmap_file() noexcept;
};

// ── Implementation ───────────────────────────────────────────────────────────

inline MemoryMappedFile::MemoryMappedFile(std::string_view file_path) { map_file(file_path); }

inline MemoryMappedFile::~MemoryMappedFile() { unmap_file(); }

inline MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : m_data(other.m_data),
      m_size(other.m_size)
#ifdef _WIN32
      ,
      m_file_handle(other.m_file_handle),
      m_mapping_handle(other.m_mapping_handle)
#else
      ,
      m_fd(other.m_fd)
#endif
{
  other.m_data = nullptr;
  other.m_size = 0;
#ifdef _WIN32
  other.m_file_handle = nullptr;
  other.m_mapping_handle = nullptr;
#else
  other.m_fd = -1;
#endif
}

inline MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
  if (this != &other) {
    unmap_file();
    m_data = other.m_data;
    m_size = other.m_size;
#ifdef _WIN32
    m_file_handle = other.m_file_handle;
    m_mapping_handle = other.m_mapping_handle;
    other.m_file_handle = nullptr;
    other.m_mapping_handle = nullptr;
#else
    m_fd = other.m_fd;
    other.m_fd = -1;
#endif
    other.m_data = nullptr;
    other.m_size = 0;
  }
  return *this;
}

inline void MemoryMappedFile::map_file(std::string_view file_path) {
#ifdef _WIN32
  // Convert string_view to std::string for Windows API
  std::string path(file_path);

  // Open the file
  m_file_handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);

  if (m_file_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to open file for memory mapping: " + path);
  }

  // Get file size
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(m_file_handle, &file_size)) {
    CloseHandle(m_file_handle);
    m_file_handle = nullptr;
    throw std::runtime_error("Failed to get file size: " + path);
  }

  if (file_size.QuadPart == 0) {
    CloseHandle(m_file_handle);
    m_file_handle = nullptr;
    m_data = nullptr;
    m_size = 0;
    return;
  }

  m_size = static_cast<std::size_t>(file_size.QuadPart);

  // Create file mapping
  m_mapping_handle = CreateFileMappingA(m_file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);

  if (m_mapping_handle == nullptr) {
    CloseHandle(m_file_handle);
    m_file_handle = nullptr;
    throw std::runtime_error("Failed to create file mapping: " + path);
  }

  // Map view of file
  m_data =
      reinterpret_cast<const std::byte*>(MapViewOfFile(m_mapping_handle, FILE_MAP_READ, 0, 0, 0));

  if (m_data == nullptr) {
    CloseHandle(m_mapping_handle);
    m_mapping_handle = nullptr;
    CloseHandle(m_file_handle);
    m_file_handle = nullptr;
    throw std::runtime_error("Failed to map view of file: " + path);
  }
#else
  // Unix implementation using mmap
  std::string path(file_path);

  // Open the file
  m_fd = open(path.c_str(), O_RDONLY);
  if (m_fd == -1) {
    throw std::runtime_error("Failed to open file for memory mapping: " + path);
  }

  // Get file size
  struct stat st;
  if (fstat(m_fd, &st) == -1) {
    close(m_fd);
    m_fd = -1;
    throw std::runtime_error("Failed to get file size: " + path);
  }

  if (st.st_size == 0) {
    close(m_fd);
    m_fd = -1;
    m_data = nullptr;
    m_size = 0;
    return;
  }

  m_size = static_cast<std::size_t>(st.st_size);

  // Map the file
  m_data =
      reinterpret_cast<const std::byte*>(mmap(nullptr, m_size, PROT_READ, MAP_SHARED, m_fd, 0));

  if (m_data == MAP_FAILED) {
    close(m_fd);
    m_fd = -1;
    m_data = nullptr;
    throw std::runtime_error("Failed to map file: " + path);
  }

  // Optional: advise kernel about access pattern (sequential read)
  madvise(const_cast<std::byte*>(m_data), m_size, MADV_SEQUENTIAL);
#endif
}

inline void MemoryMappedFile::unmap_file() noexcept {
  if (m_data == nullptr) {
    return;
  }

#ifdef _WIN32
  if (m_data != nullptr) {
    UnmapViewOfFile(m_data);
    m_data = nullptr;
  }
  if (m_mapping_handle != nullptr) {
    CloseHandle(m_mapping_handle);
    m_mapping_handle = nullptr;
  }
  if (m_file_handle != nullptr && m_file_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(m_file_handle);
    m_file_handle = nullptr;
  }
#else
  if (m_data != nullptr && m_data != MAP_FAILED) {
    munmap(const_cast<std::byte*>(m_data), m_size);
    m_data = nullptr;
  }
  if (m_fd != -1) {
    close(m_fd);
    m_fd = -1;
  }
#endif
  m_size = 0;
}

}  // namespace laspp::utilities
