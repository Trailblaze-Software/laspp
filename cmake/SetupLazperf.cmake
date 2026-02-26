# SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; version 2.1.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# For LGPL2 incompatible licensing or development requests, please contact
# trailblaze.software@gmail.com

# One-time setup of LAZperf via FetchContent.
function(setup_lazperf)
  include(FetchContent)

  # Ensure we can control the MSVC runtime via CMAKE_MSVC_RUNTIME_LIBRARY.
  if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
  endif()

  # If MSVC and the parent project hasn't set the runtime yet, default to
  # /MD(d).
  if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "" FORCE)
  endif()

  # Declare & fetch LAZperf.
  FetchContent_Declare(
    lazperf
    GIT_REPOSITORY https://github.com/hobuinc/laz-perf.git
    GIT_TAG master)
  FetchContent_MakeAvailable(lazperf)

  # LAZperf creates a target named 'lazperf' (or similar). Check what was
  # created.
  if(NOT TARGET lazperf)
    message(
      FATAL_ERROR
        "LAZperf target 'lazperf' not found after FetchContent_MakeAvailable()")
  endif()

  # Create a consistent alias
  if(NOT TARGET lazperf::lazperf)
    add_library(lazperf::lazperf ALIAS lazperf)
  endif()

  # Quiet warnings coming from LAZperf itself.
  if(NOT MSVC)
    target_compile_options(
      lazperf
      PRIVATE -Wno-maybe-uninitialized
              -Wno-format
              -Wno-format-security
              -Wno-switch
              -Wno-stringop-truncation
              -Wno-format-overflow
              -Wno-sign-conversion
              -Wno-conversion)
  endif()
endfunction()

# Helper: link target to LAZperf (and mark includes as SYSTEM to suppress
# third-party warnings). Usage: link_target_to_lazperf(my_target)
function(link_target_to_lazperf target_name)
  if(NOT TARGET lazperf::lazperf)
    message(
      FATAL_ERROR
        "lazperf::lazperf alias not found. Call setup_lazperf() first.")
  endif()

  target_link_libraries(${target_name} PRIVATE lazperf::lazperf)

  # Treat LAZperf headers as SYSTEM to avoid third-party warnings
  get_target_property(_lp_includes lazperf::lazperf
                      INTERFACE_INCLUDE_DIRECTORIES)
  if(_lp_includes)
    target_include_directories(${target_name} SYSTEM PRIVATE ${_lp_includes})
  endif()
  FetchContent_GetProperties(lazperf)
  if(NOT lazperf_POPULATED)
    message(
      FATAL_ERROR
        "lazperf FetchContent properties not found. Call setup_lazperf() first."
    )
  endif()
  # LAZperf headers are typically in cpp/ subdirectory
  target_include_directories(${target_name} SYSTEM
                             PRIVATE ${lazperf_SOURCE_DIR}/cpp)
endfunction()
