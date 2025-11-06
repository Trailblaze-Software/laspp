# SPDX-FileCopyrightText: (c) 2025 Trailblaze Software, all rights reserved
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

# One-time setup of LASzip via FetchContent.
function(setup_laszip)
  include(FetchContent)

  # Build LASzip as static so we can link it privately.
  set(LASZIP_BUILD_STATIC
      ON
      CACHE BOOL "" FORCE)

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

  # Declare & fetch LASzip.
  FetchContent_Declare(
    laszip
    GIT_REPOSITORY https://github.com/LASzip/LASzip.git
    GIT_TAG 3.4.4)
  FetchContent_MakeAvailable(laszip)

  # Normalize target names with consistent, project-local aliases so consumers
  # can just link against laszip::lib and laszip::api regardless of LASzip’s
  # internal naming.
  set(_lz_lib "")
  set(_lz_api "")

  if(TARGET laszip3)
    set(_lz_lib laszip3)
    if(TARGET laszip_api3)
      set(_lz_api laszip_api3)
    endif()
  elseif(TARGET laszip)
    set(_lz_lib laszip)
    if(TARGET laszip_api)
      set(_lz_api laszip_api)
    endif()
  else()
    message(
      FATAL_ERROR "LASzip targets not found after FetchContent_MakeAvailable()")
  endif()

  add_library(laszip::lib ALIAS ${_lz_lib})
  if(_lz_api)
    add_library(laszip::api ALIAS ${_lz_api})
  endif()

  # Quiet warnings coming from LASzip itself.
  if(NOT MSVC)
    target_compile_options(
      ${_lz_lib}
      PRIVATE -Wno-maybe-uninitialized -Wno-format -Wno-format-security
              -Wno-switch -Wno-stringop-truncation -Wno-format-overflow)
    if(_lz_api)
      target_compile_options(
        ${_lz_api}
        PRIVATE -Wno-maybe-uninitialized -Wno-format -Wno-format-security
                -Wno-switch -Wno-stringop-truncation -Wno-format-overflow)
    endif()
  endif()
endfunction()

# Helper: link target to LASzip (and mark includes as SYSTEM to suppress
# third-party warnings). Usage: link_target_to_laszip(my_target)
function(link_target_to_laszip target_name)
  if(NOT TARGET laszip::lib)
    message(
      FATAL_ERROR "laszip::lib alias not found. Call setup_laszip() first.")
  endif()

  target_link_libraries(${target_name} PRIVATE laszip::lib)

  if(TARGET laszip::api)
    target_link_libraries(${target_name} PRIVATE laszip::api)
  endif()

  # Treat LASzip headers as SYSTEM to avoid third-party warnings in your build
  # output. (Interface includes propagate from LASzip targets; we just re-mark
  # them as SYSTEM.)
  get_target_property(_lz_includes laszip::lib INTERFACE_INCLUDE_DIRECTORIES)
  if(_lz_includes)
    target_include_directories(${target_name} SYSTEM PRIVATE ${_lz_includes})
  endif()
  FetchContent_GetProperties(laszip)
  if(NOT laszip_POPULATED)
    message(
      FATAL_ERROR
        "laszip FetchContent properties not found. Call setup_laszip() first.")
  endif()
  target_include_directories(
    ${target_name} SYSTEM PRIVATE ${laszip_SOURCE_DIR}/src
                                  ${laszip_SOURCE_DIR}/dll)
endfunction()
