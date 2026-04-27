/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <cstddef>
#include <random>
#include <sstream>
#include <stdexcept>

#include "las_point.hpp"
#include "laz/laz_reader.hpp"
#include "laz/laz_vlr.hpp"
#include "laz/laz_writer.hpp"

using namespace laspp;

// Custom point type that extends LASPointFormat7 with 4 extra bytes.
// Defines copy_from in both directions so the LAZ reader and writer can
// transfer the extra bytes to/from std::vector<std::byte>.
#pragma pack(push, 1)
struct LASPointFormat7WithExtra : LASPointFormat7 {
  static constexpr uint8_t PointFormat = 7;
  static constexpr uint8_t MinVersion = 4;
  static constexpr size_t ExtraSize = 4;

  std::array<std::byte, ExtraSize> extra{};

  static LASPointFormat7WithExtra RandomData(std::mt19937_64& gen) {
    LASPointFormat7WithExtra pt{};
    static_cast<LASPointFormat7&>(pt) = LASPointFormat7::RandomData(gen);
    for (auto& b : pt.extra) b = static_cast<std::byte>(gen() & 0xFF);
    return pt;
  }

  bool operator==(const LASPointFormat7WithExtra& other) const {
    return static_cast<const LASPointFormat7&>(*this) ==
               static_cast<const LASPointFormat7&>(other) &&
           extra == other.extra;
  }
};
#pragma pack(pop)
static_assert(sizeof(LASPointFormat7WithExtra) ==
              sizeof(LASPointFormat7) + LASPointFormat7WithExtra::ExtraSize);

// Extract extra bytes from the point into a std::vector<std::byte>.
inline void copy_from(std::vector<std::byte>& dest, const LASPointFormat7WithExtra& src) {
  dest.assign(src.extra.begin(), src.extra.end());
}

// Write decoded extra bytes back into the point.
inline void copy_from(LASPointFormat7WithExtra& dest, const std::vector<std::byte>& src) {
  std::copy_n(src.begin(), std::min(src.size(), LASPointFormat7WithExtra::ExtraSize),
              dest.extra.begin());
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, 0);

        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 0);
        LASPP_ASSERT_THROWS(reader.chunk_table().chunk_offset(0), std::out_of_range);
      }
    }

    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      std::mt19937_64 gen(42);
      std::vector<LASPointFormat1> points;
      points.reserve(100);
      for (size_t i = 0; i < 100; i++) {
        points.push_back(LASPointFormat1::RandomData(gen));
      }

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point10));

        writer.write_chunk(std::span<LASPointFormat1>(points));
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      LASPP_ASSERT_EQ(stream.str().size(), 2138);

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, points.size());
        std::vector<LASPointFormat1> decompressed_points(
            reader.chunk_table().points_per_chunk()[0]);

        size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
        std::vector<std::byte> compressed_chunk(compressed_size);
        stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
        stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
                    static_cast<int64_t>(compressed_size));
        std::span<LASPointFormat1> points_span = reader.decompress_chunk(
            compressed_chunk, std::span<LASPointFormat1>(decompressed_points));

        LASPP_ASSERT_EQ(points_span.size(), points.size());
        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(static_cast<LASPointFormat0&>(points_span[i]), points[i]);
        }
      }
    }
  }
  {
    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, 0);

        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 0);
        LASPP_ASSERT_THROWS(reader.chunk_table().chunk_offset(0), std::out_of_range);
      }
    }

    {
      std::stringstream stream;
      std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

      std::mt19937_64 gen(42);
      std::vector<LASPointFormat3> points;
      points.reserve(200);
      for (size_t i = 0; i < 200; i++) {
        points.push_back(LASPointFormat3::RandomData(gen));
      }

      {
        LAZWriter writer(stream, LAZCompressor::PointwiseChunked);
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::GPSTime11));
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGB12));
        writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point10));

        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(0, 10));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(10, 20));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(30, 120));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(150, 1));
        writer.write_chunk(std::span<LASPointFormat3>(points).subspan(151, 49));
        laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
      }

      LASPP_ASSERT_EQ(stream.str().size(), 7213);

      {
        LAZReader reader(*laz_special_vlr);
        reader.read_chunk_table(stream, points.size());
        LASPP_ASSERT_EQ(reader.chunk_table().num_chunks(), 5);

        std::vector<LASPointFormat3> decompressed_points(200);

        for (size_t i = 0; i < reader.chunk_table().num_chunks(); i++) {
          size_t compressed_size = reader.chunk_table().compressed_chunk_size(i);
          std::vector<std::byte> compressed_chunk(compressed_size);
          stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(i)));
          stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
                      static_cast<int64_t>(compressed_size));
          reader.decompress_chunk(compressed_chunk,
                                  std::span<LASPointFormat3>(decompressed_points)
                                      .subspan(reader.chunk_table().decompressed_chunk_offsets()[i],
                                               reader.chunk_table().points_per_chunk()[i]));
        }

        for (size_t i = 0; i < points.size(); i++) {
          LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
        }
      }
    }
  }
  {
    std::stringstream stream;
    std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

    std::mt19937_64 gen(2024);
    std::vector<LASPointFormat7> points;
    points.reserve(150);
    for (size_t i = 0; i < 150; i++) {
      points.push_back(LASPointFormat7::RandomData(gen));
    }

    {
      LAZWriter writer(stream, LAZCompressor::LayeredChunked);
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point14));
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGB14));

      writer.write_chunk(std::span<LASPointFormat7>(points));
      laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
    }

    LASPP_ASSERT_EQ(stream.str().size(), 5929);

    {
      LAZReader reader(*laz_special_vlr);
      reader.read_chunk_table(stream, points.size());
      std::vector<LASPointFormat7> decompressed_points(points.size());

      size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
      std::vector<std::byte> compressed_chunk(compressed_size);
      stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
      stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
                  static_cast<int64_t>(compressed_size));
      reader.decompress_chunk(compressed_chunk, std::span<LASPointFormat7>(decompressed_points));

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
      }
    }
  }
  // Byte14 (LAS 1.4 extra bytes) roundtrip test
  {
    std::stringstream stream;
    std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

    std::mt19937_64 gen(999);
    std::vector<LASPointFormat7WithExtra> points;
    points.reserve(150);
    for (size_t i = 0; i < 150; i++) {
      points.push_back(LASPointFormat7WithExtra::RandomData(gen));
    }

    {
      LAZWriter writer(stream, LAZCompressor::LayeredChunked);
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point14));
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGB14));
      writer.special_vlr().add_item_record(
          LAZItemRecord(LAZItemType::Byte14, LASPointFormat7WithExtra::ExtraSize));

      writer.write_chunk(std::span<LASPointFormat7WithExtra>(points));
      laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
    }

    {
      LAZReader reader(*laz_special_vlr);
      reader.read_chunk_table(stream, points.size());
      std::vector<LASPointFormat7WithExtra> decompressed_points(points.size());

      size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
      std::vector<std::byte> compressed_chunk(compressed_size);
      stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
      stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
                  static_cast<int64_t>(compressed_size));
      reader.decompress_chunk(compressed_chunk,
                              std::span<LASPointFormat7WithExtra>(decompressed_points));

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
      }
    }
  }

  // RGBNIR14 (LAS 1.4 point format 8) roundtrip test
  {
    std::stringstream stream;
    std::unique_ptr<LAZSpecialVLRContent> laz_special_vlr;

    std::mt19937_64 gen(42424242);
    std::vector<LASPointFormat8> points;
    points.reserve(150);
    for (size_t i = 0; i < 150; i++) {
      points.push_back(LASPointFormat8::RandomData(gen));
    }

    {
      LAZWriter writer(stream, LAZCompressor::LayeredChunked);
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::Point14));
      writer.special_vlr().add_item_record(LAZItemRecord(LAZItemType::RGBNIR14));

      writer.write_chunk(std::span<LASPointFormat8>(points));
      laz_special_vlr = std::make_unique<LAZSpecialVLRContent>(writer.special_vlr());
    }

    {
      LAZReader reader(*laz_special_vlr);
      reader.read_chunk_table(stream, points.size());
      std::vector<LASPointFormat8> decompressed_points(points.size());

      size_t compressed_size = reader.chunk_table().compressed_chunk_size(0);
      std::vector<std::byte> compressed_chunk(compressed_size);
      stream.seekg(static_cast<int64_t>(reader.chunk_table().chunk_offset(0)));
      stream.read(reinterpret_cast<char*>(compressed_chunk.data()),
                  static_cast<int64_t>(compressed_size));
      reader.decompress_chunk(compressed_chunk, std::span<LASPointFormat8>(decompressed_points));

      for (size_t i = 0; i < points.size(); i++) {
        LASPP_ASSERT_EQ(decompressed_points[i], points[i]);
      }
    }
  }

  return 0;
}
