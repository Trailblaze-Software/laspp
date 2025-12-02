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

set(INSPECT_VLRS_EXE_NAME las++-inspect-vlrs)

add_executable(${INSPECT_VLRS_EXE_NAME} inspect_vlrs.cpp)

target_link_libraries(${INSPECT_VLRS_EXE_NAME} ${LIBRARY_NAME}
                      OpenMP::OpenMP_CXX)

set(VALIDATE_SPATIAL_INDEX_EXE_NAME las++-validate-spatial-index)

add_executable(${VALIDATE_SPATIAL_INDEX_EXE_NAME} validate_spatial_index.cpp)

target_link_libraries(${VALIDATE_SPATIAL_INDEX_EXE_NAME} ${LIBRARY_NAME}
                      OpenMP::OpenMP_CXX)

install(
  TARGETS ${LAS2LAS++_EXE_NAME} ${INSPECT_VLRS_EXE_NAME}
          ${VALIDATE_SPATIAL_INDEX_EXE_NAME}
  DESTINATION bin
  COMPONENT applications)

if(LASPP_BUILD_TESTS)
  # Test application for comparing LAS++ and LASzip
  set(TEST_LASPP_LASZIP_EXE_NAME test_laspp_laszip)

  add_executable(${TEST_LASPP_LASZIP_EXE_NAME} test_laspp_laszip.cpp)

  target_link_libraries(${TEST_LASPP_LASZIP_EXE_NAME}
                        PRIVATE ${LIBRARY_NAME} OpenMP::OpenMP_CXX)

  link_target_to_laszip(${TEST_LASPP_LASZIP_EXE_NAME})

  install(
    TARGETS ${TEST_LASPP_LASZIP_EXE_NAME}
    DESTINATION bin
    COMPONENT applications)
endif()
