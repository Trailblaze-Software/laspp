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
