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

set(LAS2LAS++_EXE_NAME las2las++)

add_executable(${LAS2LAS++_EXE_NAME} las2las++.cpp)

target_link_libraries(${LAS2LAS++_EXE_NAME} ${LIBRARY_NAME} OpenMP::OpenMP_CXX)

install(
  TARGETS ${LAS2LAS++_EXE_NAME}
  DESTINATION bin
  COMPONENT applications)

if(LASPP_BUILD_TESTS)
  # Test application for comparing LAS++ and LASzip
  set(TEST_LASPP_LASZIP_EXE_NAME test_laspp_laszip)

  add_executable(${TEST_LASPP_LASZIP_EXE_NAME} test_laspp_laszip.cpp)

  target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME}
                        PRIVATE ${LIBRARY_NAME} OpenMP::OpenMP_CXX)

  # Link against laszip if available
  if(TARGET laszip3)
    target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME} PRIVATE laszip3)
    if(TARGET laszip_api3)
      target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME} PRIVATE laszip_api3)
    endif()
  elseif(TARGET laszip)
    target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME} PRIVATE laszip)
    if(TARGET laszip_api)
      target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME} PRIVATE laszip_api)
    endif()
  endif()

  # Include directories for laszip
  if(EXISTS ${laszip_SOURCE_DIR})
    target_include_directories(
      ${TEST_LASPP_LASZIP_EXE_NAME} SYSTEM
      PRIVATE ${laszip_SOURCE_DIR}/src ${laszip_SOURCE_DIR}/dll
              ${laszip_BINARY_DIR}/include)
  endif()

  install(
    TARGETS ${TEST_LASPP_LASZIP_EXE_NAME}
    DESTINATION bin
    COMPONENT applications)
endif()
