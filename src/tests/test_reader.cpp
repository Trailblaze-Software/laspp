
/*
 * SPDX-FileCopyrightText: (c) 2024-2025 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024-2025 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#include "las_reader.hpp"
#include "las_writer.hpp"
using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);

      std::vector<LASPointFormat0> points;
      points.reserve(100);
      for (size_t i = 0; i < points.capacity(); i++) {
        points.emplace_back();
        points.back().x = static_cast<int32_t>(i);
      }
      writer.write_points(std::span<LASPointFormat0>(points));

      LASPP_ASSERT_THROWS(writer.write_vlr(LASVLR(), std::vector<std::byte>()), std::runtime_error);
    }

    {
      LASReader reader(stream);
    }
  }

  return 0;
}
