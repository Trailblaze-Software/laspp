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

set(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -pg")

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
      "/wd5026")
  endif()
elseif(UNIX)
  add_compile_options("-fstrict-enums" "-Wall" "-Wextra" "-Wpedantic" "-Werror")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

set(LIBRARY_NAME las++)
set(EXE_NAME las++-cli)

include_directories(src)

add_library(${LIBRARY_NAME} INTERFACE)
add_executable(${EXE_NAME})

find_package(OpenMP REQUIRED)

target_link_libraries(${LIBRARY_NAME} INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(${EXE_NAME} PRIVATE ${LIBRARY_NAME})

add_subdirectory(src)
if(LASPP_BUILD_APPS)
  add_subdirectory(apps)
endif()

# if(LASPP_BUILD_TESTS) enable_testing()
#
# add_executable(las++-tests) target_link_libraries(las++-tests PRIVATE
# ${LIBRARY_NAME}) endif()

include(GNUInstallDirs)

install(TARGETS ${EXE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  TARGETS ${LIBRARY_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
