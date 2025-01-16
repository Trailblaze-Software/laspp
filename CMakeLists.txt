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

add_compile_definitions(LASPP_VERSION="${PROJECT_VERSION}")
if(LASPP_DEBUG_ASSERTS)
  message(WARNING "Enabling LAS++ debugging asserts. May be slow.")
  add_compile_definitions(LASPP_DEBUG_ASSERTS)
endif()

if(LASPP_BUILD_TESTS)
  # include(FetchContent) FetchContent_Declare( laszip GIT_REPOSITORY
  # https://github.com/LASzip/LASzip.git GIT_TAG 3.4.4)
  # FetchContent_MakeAvailable(laszip)
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
      "/openmp:llvm"
      "/Wall"
      "/MP"
      "/wd5045"
      "/wd4866"
      "/wd4514"
      "/wd4820"
      "/wd4800"
      "/wd4242"
      "/wd4244"
      "/wd4365"
      "/wd5027"
      "/wd4626"
      "/wd5219"
      "/wd4623"
      "/wd4267"
      "/wd4710"
      "/wd4711"
      "/wd4625"
      "/wd5026"
      "/wd5246")
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

include_directories(src)

add_library(${LIBRARY_NAME} INTERFACE)

find_package(OpenMP REQUIRED)

target_link_libraries(${LIBRARY_NAME} INTERFACE OpenMP::OpenMP_CXX)

add_subdirectory(src)
if(LASPP_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(LASPP_BUILD_TESTS)
  include(CTest)
  enable_testing()

  file(GLOB_RECURSE TEST_SOURCES "test*.cpp")
  message(STATUS "Test sources: ${TEST_SOURCES}")

  foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE ${LIBRARY_NAME})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    target_compile_definitions(${TEST_NAME} PRIVATE LASPP_DEBUG_ASSERTS)
  endforeach()

endif()

include(GNUInstallDirs)

install(
  TARGETS ${LIBRARY_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
