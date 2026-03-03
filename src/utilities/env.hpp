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

#include <cstdlib>
#include <optional>
#include <string>
#ifdef _WIN32
#include <memory>
#endif

namespace laspp {
namespace utilities {

// Returns the value of the named environment variable, or std::nullopt if it is not set.
inline std::optional<std::string> get_env(const char* name) {
#ifdef _WIN32
  char* buf = nullptr;
  size_t len = 0;
  std::unique_ptr<char, decltype(&free)> guard(nullptr, &free);
  if (_dupenv_s(&buf, &len, name) == 0 && buf != nullptr) {
    guard.reset(buf);
    return std::string(buf);
  }
  return std::nullopt;
#else
  const char* val = std::getenv(name);
  if (val != nullptr) {
    return std::string(val);
  }
  return std::nullopt;
#endif
}

}  // namespace utilities
}  // namespace laspp
