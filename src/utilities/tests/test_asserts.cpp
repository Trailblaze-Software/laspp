/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: LGPL-2.1
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#include "utilities/assert.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  LASPP_ASSERT_EQ(1, 1);
  LASPP_ASSERT(true);

  LASPP_ASSERT_THROWS({ LASPP_ASSERT_EQ(1, 2); }, std::runtime_error);
  LASPP_ASSERT_THROWS({ LASPP_ASSERT(false, "Amazing"); }, std::runtime_error);
  LASPP_ASSERT_THROWS({ LASPP_ASSERT_RAW_STR_EQ("hello", "world"); }, std::runtime_error);

  return 0;
}
