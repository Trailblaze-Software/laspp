# SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
# SPDX-License-Identifier: MIT

set(LAS2LAS++_EXE_NAME las2las++)

add_executable(${LAS2LAS++_EXE_NAME} las2las++.cpp)

target_link_libraries(${LAS2LAS++_EXE_NAME} ${LIBRARY_NAME})

# Utility to create test files for benchmarking
add_executable(create_test_file create_test_file.cpp)
target_link_libraries(create_test_file PRIVATE ${LIBRARY_NAME})

install(
  TARGETS ${LAS2LAS++_EXE_NAME}
  DESTINATION bin
  COMPONENT applications)

if(LASPP_BUILD_TESTS)
  # Test application for comparing LAS++ and LASzip
  set(TEST_LASPP_LASZIP_EXE_NAME test_laspp_laszip)

  add_executable(${TEST_LASPP_LASZIP_EXE_NAME} test_laspp_laszip.cpp)

  target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME} PRIVATE ${LIBRARY_NAME})

  link_target_to_laszip(${TEST_LASPP_LASZIP_EXE_NAME})

  install(
    TARGETS ${TEST_LASPP_LASZIP_EXE_NAME}
    DESTINATION bin
    COMPONENT applications)
endif()

if(LASPP_BUILD_BENCHMARK)
  # Performance benchmark comparing LAS++ (multi-threaded) vs laszip, with JSON
  # output consumed by the companion benchmark.py Python script.
  set(BENCHMARK_EXE_NAME benchmark)

  add_executable(${BENCHMARK_EXE_NAME} benchmark.cpp)

  target_link_libraries(${BENCHMARK_EXE_NAME} PRIVATE ${LIBRARY_NAME})

  link_target_to_laszip(${BENCHMARK_EXE_NAME})
  link_target_to_lazperf(${BENCHMARK_EXE_NAME})

  # Define LASPP_HAS_LAZPERF so benchmark.cpp knows lazperf is available
  target_compile_definitions(${BENCHMARK_EXE_NAME} PRIVATE LASPP_HAS_LAZPERF)

  # On Windows, copy lazperf DLL to output directory so benchmark can find it
  if(WIN32 AND TARGET lazperf::lazperf)
    FetchContent_GetProperties(lazperf)
    if(lazperf_POPULATED)
      add_custom_command(
        TARGET ${BENCHMARK_EXE_NAME}
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:lazperf::lazperf>
          $<TARGET_FILE_DIR:${BENCHMARK_EXE_NAME}>)
    endif()
  endif()

  install(
    TARGETS ${BENCHMARK_EXE_NAME}
    DESTINATION bin
    COMPONENT applications)
endif()
