
/*
 * SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
 *
 * For LGPL2 incompatible licensing or development requests, please contact
 * trailblaze.software@gmail.com
 */

#include "laz/laz_vlr.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  {
    std::stringstream stream;
    {
      LAZSpecialVLR laz_vlr(LAZCompressor::None);
      laz_vlr.write_to(stream);
    }

    {
      LAZSpecialVLR laz_vlr(stream);

      LASPP_ASSERT_EQ(laz_vlr.compressor, LAZCompressor::None);
      LASPP_ASSERT(laz_vlr.items_records.empty());
      LASPP_ASSERT_EQ(laz_vlr.coder, 0);
      LASPP_ASSERT_EQ(laz_vlr.version_major, 1);
      LASPP_ASSERT_EQ(laz_vlr.version_minor, 4);
      LASPP_ASSERT_EQ(laz_vlr.version_revision, 0);
      LASPP_ASSERT_EQ(laz_vlr.compatibility_mode, false);
      LASPP_ASSERT_EQ(laz_vlr.options_reserved, 0);
      LASPP_ASSERT_EQ(laz_vlr.chunk_size, 0);
      LASPP_ASSERT_EQ(laz_vlr.num_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.offset_of_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.num_item_records, 0);
    }
  }

  {
    std::stringstream stream;
    {
      LAZSpecialVLR laz_vlr(LAZCompressor::None);
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Point10));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::RGB12));
      LASPP_ASSERT_THROWS(laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Point10, 3)),
                          std::runtime_error);
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Byte));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Byte, 12));
      laz_vlr.write_to(stream);
    }

    {
      LAZSpecialVLR laz_vlr(stream);

      LASPP_ASSERT_EQ(laz_vlr.compressor, LAZCompressor::None);
      LASPP_ASSERT_EQ(laz_vlr.items_records.size(), 4);
      LASPP_ASSERT_EQ(laz_vlr.coder, 0);
      LASPP_ASSERT_EQ(laz_vlr.version_major, 1);
      LASPP_ASSERT_EQ(laz_vlr.version_minor, 4);
      LASPP_ASSERT_EQ(laz_vlr.version_revision, 0);
      LASPP_ASSERT_EQ(laz_vlr.compatibility_mode, false);
      LASPP_ASSERT_EQ(laz_vlr.options_reserved, 0);
      LASPP_ASSERT_EQ(laz_vlr.chunk_size, 0);
      LASPP_ASSERT_EQ(laz_vlr.num_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.offset_of_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.num_item_records, 4);

      LASPP_ASSERT_EQ(laz_vlr.items_records[0].item_type, LAZItemType::Point10);
      LASPP_ASSERT_EQ(laz_vlr.items_records[0].item_size, 20);
      LASPP_ASSERT_EQ(laz_vlr.items_records[1].item_type, LAZItemType::RGB12);
      LASPP_ASSERT_EQ(laz_vlr.items_records[1].item_size, 6);
      LASPP_ASSERT_EQ(laz_vlr.items_records[2].item_type, LAZItemType::Byte);
      LASPP_ASSERT_EQ(laz_vlr.items_records[2].item_size, 1);
      LASPP_ASSERT_EQ(laz_vlr.items_records[3].item_type, LAZItemType::Byte);
      LASPP_ASSERT_EQ(laz_vlr.items_records[3].item_size, 12);
    }
  }

  return 0;
}
