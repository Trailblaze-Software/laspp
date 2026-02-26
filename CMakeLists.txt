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

cmake_minimum_required(VERSION 3.20)

option(LASPP_DEBUG_ASSERTS "Enable debugging asserts" ON)
option(LASPP_BUILD_TESTS "Build tests" ON)
option(LASPP_BUILD_APPS "Build apps" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
  LAS++
  VERSION 0.0.0
  LANGUAGES CXX)
set(COPYRIGHT "Copyright (c) 2024 Trailblaze Software. All rights reserved.")

if(LASPP_BUILD_TESTS)
  include(cmake/SetupLaszip.cmake)
  setup_laszip()
  include(cmake/SetupLazperf.cmake)
  setup_lazperf()
endif()

set(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -pg")

set(CMAKE_CXX_FLAGS_COVERAGE
    "-O0 -g -coverage -fprofile-arcs -ftest-coverage -fno-elide-constructors")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_COVERAGE
      "${CMAKE_CXX_FLAGS_COVERAGE} -fno-default-inline")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
  if(${CMAKE_CXX_COMPILER} MATCHES ".*mingw.*")
    add_compile_options("-fstrict-enums" "-Wall" "-Wextra" "-Wpedantic"
                        "-Werror")
  else()
    add_compile_options(
      "/W4"
      "/WX"
      # C4267/C4244: size_t/type narrowing inside MSVC STL template bodies.
      "/wd4267" # size_t narrowing in MSVC STL <xutility> template
                # instantiations
      "/wd4244") # type narrowing in MSVC STL <xutility> template instantiations
  endif()
elseif(UNIX)
  if(NOT CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_options("-Werror")
  endif()
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options("-fsanitize=address,undefined"
                        "-fno-sanitize-recover=all")
    add_link_options("-fsanitize=address,undefined")
  endif()
  add_compile_options(
    "-fstrict-enums"
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-Wdouble-promotion"
    "-Wshadow"
    "-Wpointer-arith"
    "-Wstrict-aliasing"
    "-Wnull-dereference"
    "-Wdouble-promotion"
    "-Wold-style-cast"
    "-Wconversion"
    "-Wsign-conversion")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
      "-Wduplicated-branches" "-Wlogical-op" "-Wrestrict" "-Wcast-align=strict"
      "-Wduplicated-cond" "-Wuseless-cast")
  endif()
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

set(LIBRARY_NAME las++)

add_library(${LIBRARY_NAME} INTERFACE)

target_include_directories(${LIBRARY_NAME} INTERFACE src)

if(LASPP_DEBUG_ASSERTS)
  message(WARNING "Enabling LAS++ debugging asserts. May be slow.")
  target_compile_definitions(${LIBRARY_NAME} INTERFACE LASPP_DEBUG_ASSERTS)
endif()

# TBB is required on Unix, optional on Windows (for thread control in
# benchmarks)
if(UNIX)
  find_package(TBB REQUIRED)
  target_link_libraries(${LIBRARY_NAME} INTERFACE TBB::tbb)
elseif(WIN32)
  # On Windows, try to find TBB for better thread control If found, the library
  # will use TBB parallel_for instead of std::execution
  find_package(TBB QUIET)
  if(TBB_FOUND)
    target_link_libraries(${LIBRARY_NAME} INTERFACE TBB::tbb)
  endif()
endif()
target_compile_definitions(${LIBRARY_NAME}
                           INTERFACE LASPP_VERSION="${PROJECT_VERSION}")

add_subdirectory(src)
if(LASPP_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(LASPP_BUILD_TESTS)
  include(CTest)
  enable_testing()

  file(GLOB_RECURSE UNFILTERED_TEST_SOURCES "${CMAKE_SOURCE_DIR}/*test*.cpp")

  # Filter to only those under a 'tests' directory
  set(SOURCES "")
  foreach(path ${UNFILTERED_TEST_SOURCES})
    if(path MATCHES "/tests/")
      list(APPEND TEST_SOURCES "${path}")
    endif()
  endforeach()
  message(STATUS "Test sources: ${TEST_SOURCES}")

  foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE ${LIBRARY_NAME})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    target_compile_definitions(${TEST_NAME} PRIVATE LASPP_DEBUG_ASSERTS)
  endforeach()

  if(TARGET test_laszip_interop)
    FetchContent_GetProperties(laszip)
    if(NOT laszip_POPULATED)
      message(
        FATAL_ERROR
          "laszip FetchContent properties not found. Call setup_laszip() first."
      )
    endif()
    # laszipper.cpp and lasunzipper.cpp are wrapper files that need internal
    # laszip symbols
    file(GLOB LASZIP_WRAPPER_SOURCES "${laszip_SOURCE_DIR}/src/laszipper.cpp"
         "${laszip_SOURCE_DIR}/src/lasunzipper.cpp")

    # Add wrapper files as sources to the test and suppress their warnings:
    # these are third-party laszip files, not laspp code.
    target_sources(test_laszip_interop PRIVATE ${LASZIP_WRAPPER_SOURCES})
    if(MSVC)
      set_source_files_properties(${LASZIP_WRAPPER_SOURCES}
                                  PROPERTIES COMPILE_FLAGS "/W0")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set_source_files_properties(
        ${LASZIP_WRAPPER_SOURCES}
        PROPERTIES COMPILE_FLAGS
                   "-Wno-old-style-cast -Wno-shadow -Wno-sign-conversion \
-Wno-conversion -Wno-useless-cast")
    else()
      set_source_files_properties(
        ${LASZIP_WRAPPER_SOURCES}
        PROPERTIES COMPILE_FLAGS
                   "-Wno-old-style-cast -Wno-shadow -Wno-sign-conversion \
-Wno-conversion")
    endif()

    link_target_to_laszip(test_laszip_interop)
  endif()
endif()

include(GNUInstallDirs)

install(
  TARGETS ${LIBRARY_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
