
/*
 * SPDX-FileCopyrightText: (c) 2025 Trailblaze Software, all rights reserved
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
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Short));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Double));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Float));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::GPSTime11));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Integer));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Long));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Point14));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::RGB14));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::RGBNIR14));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Wavepacket13));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Wavepacket14));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Byte14));
      laz_vlr.add_item_record(LAZItemRecord(LAZItemType::Byte14, 17));

      for (const auto &item_record : laz_vlr.items_records) {
        std::cout << item_record << std::endl;
      }
      for (const auto &LAZItemType : {LAZItemType::Short, LAZItemType::Integer, LAZItemType::Long,
                                      LAZItemType::Float, LAZItemType::Double}) {
        LASPP_ASSERT_THROWS(laz_vlr.add_item_record(LAZItemRecord(LAZItemType, 1)),
                            std::runtime_error);
      }
      LASPP_ASSERT_THROWS(laz_vlr.add_item_record(LAZItemRecord(static_cast<LAZItemType>(15))),
                          std::runtime_error);
      laz_vlr.write_to(stream);
    }

    {
      LAZSpecialVLR laz_vlr(stream);

      LASPP_ASSERT_EQ(laz_vlr.compressor, LAZCompressor::None);
      LASPP_ASSERT_EQ(laz_vlr.items_records.size(), 17);
      LASPP_ASSERT_EQ(laz_vlr.coder, 0);
      LASPP_ASSERT_EQ(laz_vlr.version_major, 1);
      LASPP_ASSERT_EQ(laz_vlr.version_minor, 4);
      LASPP_ASSERT_EQ(laz_vlr.version_revision, 0);
      LASPP_ASSERT_EQ(laz_vlr.compatibility_mode, false);
      LASPP_ASSERT_EQ(laz_vlr.options_reserved, 0);
      LASPP_ASSERT_EQ(laz_vlr.chunk_size, 0);
      LASPP_ASSERT_EQ(laz_vlr.num_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.offset_of_special_evlrs, -1);
      LASPP_ASSERT_EQ(laz_vlr.num_item_records, 17);

      LASPP_ASSERT_EQ(laz_vlr.items_records[0].item_type, LAZItemType::Point10);
      LASPP_ASSERT_EQ(laz_vlr.items_records[0].item_size, 20);
      LASPP_ASSERT_EQ(laz_vlr.items_records[1].item_type, LAZItemType::RGB12);
      LASPP_ASSERT_EQ(laz_vlr.items_records[1].item_size, 6);
      LASPP_ASSERT_EQ(laz_vlr.items_records[2].item_type, LAZItemType::Byte);
      LASPP_ASSERT_EQ(laz_vlr.items_records[2].item_size, 1);
      LASPP_ASSERT_EQ(laz_vlr.items_records[3].item_type, LAZItemType::Byte);
      LASPP_ASSERT_EQ(laz_vlr.items_records[3].item_size, 12);
      LASPP_ASSERT_EQ(laz_vlr.items_records[4].item_type, LAZItemType::Short);
      LASPP_ASSERT_EQ(laz_vlr.items_records[4].item_size, 2);
      LASPP_ASSERT_EQ(laz_vlr.items_records[5].item_type, LAZItemType::Double);
      LASPP_ASSERT_EQ(laz_vlr.items_records[5].item_size, 8);
      LASPP_ASSERT_EQ(laz_vlr.items_records[6].item_type, LAZItemType::Float);
      LASPP_ASSERT_EQ(laz_vlr.items_records[6].item_size, 4);
      LASPP_ASSERT_EQ(laz_vlr.items_records[7].item_type, LAZItemType::GPSTime11);
      LASPP_ASSERT_EQ(laz_vlr.items_records[7].item_size, 8);
      LASPP_ASSERT_EQ(laz_vlr.items_records[8].item_type, LAZItemType::Integer);
      LASPP_ASSERT_EQ(laz_vlr.items_records[8].item_size, 4);
      LASPP_ASSERT_EQ(laz_vlr.items_records[9].item_type, LAZItemType::Long);
      LASPP_ASSERT_EQ(laz_vlr.items_records[9].item_size, 8);
      LASPP_ASSERT_EQ(laz_vlr.items_records[10].item_type, LAZItemType::Point14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[10].item_size, 30);
      LASPP_ASSERT_EQ(laz_vlr.items_records[11].item_type, LAZItemType::RGB14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[11].item_size, 6);
      LASPP_ASSERT_EQ(laz_vlr.items_records[12].item_type, LAZItemType::RGBNIR14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[12].item_size, 8);
      LASPP_ASSERT_EQ(laz_vlr.items_records[13].item_type, LAZItemType::Wavepacket13);
      LASPP_ASSERT_EQ(laz_vlr.items_records[13].item_size, 29);
      LASPP_ASSERT_EQ(laz_vlr.items_records[14].item_type, LAZItemType::Wavepacket14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[14].item_size, 29);
      LASPP_ASSERT_EQ(laz_vlr.items_records[15].item_type, LAZItemType::Byte14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[15].item_size, 1);
      LASPP_ASSERT_EQ(laz_vlr.items_records[16].item_type, LAZItemType::Byte14);
      LASPP_ASSERT_EQ(laz_vlr.items_records[16].item_size, 17);

      LASPP_ASSERT_THROWS(laz_vlr.add_item_record(LAZItemRecord(static_cast<LAZItemType>(15))),
                          std::runtime_error);
    }
  }

  return 0;
}
