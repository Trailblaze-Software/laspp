/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#include <optional>
#include <sstream>
#include <vector>

#include "las_header.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "utilities/assert.hpp"
#include "vlr.hpp"

using namespace laspp;

namespace {

void write_projection_vlr(LASWriter& writer, uint16_t record_id,
                          std::span<const std::byte> payload) {
  LASVLR vlr{};
  vlr.reserved = 0;
  string_to_arr("LASF_Projection", vlr.user_id);
  vlr.record_id = record_id;
  vlr.record_length_after_header = static_cast<uint16_t>(payload.size());
  string_to_arr("GeoKeys", vlr.description);
  writer.write_vlr(vlr, payload);
}

template <typename T>
void append_object_bytes(std::vector<std::byte>& out, const T& value) {
  const auto bytes = std::as_bytes(std::span(&value, 1));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

std::vector<std::byte> make_geo_key_directory_payload(uint16_t key_id, TIFFTagLocation location,
                                                      uint16_t count, uint16_t value_offset) {
  sGeoKeys header{};
  header.wKeyDirectoryVersion = 1;
  header.wKeyRevision = 1;
  header.wMinorRevision = 0;
  header.wNumberOfKeys = 1;

  sGeoKeys::sKeyEntry entry{};
  entry.wKeyID = key_id;
  entry.wTIFFTagLocation = location;
  entry.wCount = count;
  entry.wValue_Offset = value_offset;

  std::vector<std::byte> payload;
  payload.reserve(sizeof(sGeoKeys) + sizeof(sGeoKeys::sKeyEntry));
  append_object_bytes(payload, header);
  append_object_bytes(payload, entry);
  return payload;
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  {
    GeoKeyValue u16_value(32633);
    LASPP_ASSERT(u16_value.type == GeoKeyValue::Type::U16);
    LASPP_ASSERT_EQ(u16_value.u16, 32633u);

    GeoKeyValue doubles_value(std::vector<double>{1.0, 2.5, 3.0});
    LASPP_ASSERT(doubles_value.type == GeoKeyValue::Type::Doubles);
    LASPP_ASSERT_EQ(doubles_value.doubles.size(), 3u);
    LASPP_ASSERT_EQ(doubles_value.doubles[1], 2.5);

    GeoKeyValue string_value(std::string("EPSG:32633"));
    LASPP_ASSERT(string_value.type == GeoKeyValue::Type::String);
    LASPP_ASSERT_EQ(string_value.str, "EPSG:32633");
  }

  {
    LASGeoKeys geo_keys(1, 1, 0);
    geo_keys.add_key(3072, uint16_t(32633));
    geo_keys.add_key(1024, std::vector<double>{0.0, 500000.0, 0.0, 500000.0});
    geo_keys.add_key(2049, std::string("NAD83 / UTM zone 17N"));

    LASPP_ASSERT_EQ(geo_keys.get_keys().size(), 3u);

    const GeoKeyValue& projected = geo_keys.get_key(3072);
    LASPP_ASSERT(projected.type == GeoKeyValue::Type::U16);
    LASPP_ASSERT_EQ(projected.u16, 32633u);

    const GeoKeyValue& doubles = geo_keys.get_key(1024);
    LASPP_ASSERT(doubles.type == GeoKeyValue::Type::Doubles);
    LASPP_ASSERT_EQ(doubles.doubles.size(), 4u);
    LASPP_ASSERT_EQ(doubles.doubles[2], 0.0);

    const GeoKeyValue& name = geo_keys.get_key(2049);
    LASPP_ASSERT(name.type == GeoKeyValue::Type::String);
    LASPP_ASSERT_EQ(name.str, "NAD83 / UTM zone 17N");

    geo_keys.add_key(3072, uint16_t(4326));
    LASPP_ASSERT_EQ(geo_keys.get_key(3072).u16, 4326u);
  }

  {
    LASGeoKeys original(1, 0, 0);
    original.add_key(2048, uint16_t(4326));
    const LASGeoKeys copy = original;

    LASPP_ASSERT(copy.get_key(2048).type == GeoKeyValue::Type::U16);
    LASPP_ASSERT_EQ(copy.get_key(2048).u16, 4326u);
  }

  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));

      const auto directory =
          make_geo_key_directory_payload(3072, TIFFTagLocation::UnsignedShort, 1, 32633);
      write_projection_vlr(writer, 34735, directory);

      std::vector<LASPointFormat0> points(1);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    LASReader reader(stream);
    LASPP_ASSERT(reader.geo_keys().has_value());
    const LASGeoKeys parsed = reader.geo_keys().value();
    LASPP_ASSERT_EQ(parsed.get_keys().size(), 1u);
    LASPP_ASSERT(parsed.get_key(3072).type == GeoKeyValue::Type::U16);
    LASPP_ASSERT_EQ(parsed.get_key(3072).u16, 32633u);
  }

  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));

      const auto directory =
          make_geo_key_directory_payload(1024, TIFFTagLocation::GeoDoubleParams, 2, 0);
      write_projection_vlr(writer, 34735, directory);

      const std::vector<double> params{10.0, 20.0, 30.0};
      const auto doubles_payload = std::as_bytes(std::span<const double>(params));
      write_projection_vlr(writer, 34736,
                           std::vector<std::byte>(doubles_payload.begin(), doubles_payload.end()));

      std::vector<LASPointFormat0> points(1);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    LASReader reader(stream);
    LASPP_ASSERT(reader.geo_keys().has_value());
    const std::optional<LASGeoKeys> geo_keys = reader.geo_keys();
    LASPP_ASSERT(geo_keys.has_value());
    const GeoKeyValue& value = geo_keys->get_key(1024);
    LASPP_ASSERT(value.type == GeoKeyValue::Type::Doubles);
    LASPP_ASSERT_EQ(value.doubles.size(), 2u);
    LASPP_ASSERT_EQ(value.doubles[0], 10.0);
    LASPP_ASSERT_EQ(value.doubles[1], 20.0);
  }

  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      writer.write_vlr(LASVLR(), std::vector<std::byte>(0));

      const auto directory =
          make_geo_key_directory_payload(2049, TIFFTagLocation::GeoAsciiParams, 4, 0);
      write_projection_vlr(writer, 34735, directory);

      const std::string ascii = "EPSG";
      write_projection_vlr(
          writer, 34737,
          std::vector<std::byte>(reinterpret_cast<const std::byte*>(ascii.data()),
                                 reinterpret_cast<const std::byte*>(ascii.data() + ascii.size())));

      std::vector<LASPointFormat0> points(1);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    LASReader reader(stream);
    const std::optional<LASGeoKeys> geo_keys = reader.geo_keys();
    LASPP_ASSERT(geo_keys.has_value());
    const GeoKeyValue& value = geo_keys->get_key(2049);
    LASPP_ASSERT(value.type == GeoKeyValue::Type::String);
    LASPP_ASSERT_EQ(value.str, "EPSG");
  }

  return 0;
}
