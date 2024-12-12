#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <variant>

#include "las_point.hpp"
#include "laz/byte_encoder.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/stream.hpp"
#include "lazchunktable.hpp"
#include "lazvlr.hpp"

namespace laspp {

class PointerStreamBuffer : public std::streambuf {
 public:
  PointerStreamBuffer(std::byte* data, size_t size) {
    setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data),
         reinterpret_cast<char*>(data + size));
  }
};

typedef std::variant<LASPointFormat0Encoder, GPSTime11Encoder, BytesEncoder> LAZEncoder;

class LAZReader {
  LAZSpecialVLR m_special_vlr;

 public:  // make private
  std::optional<LAZChunkTable> m_chunk_table;

 public:
  explicit LAZReader(const LAZSpecialVLR& special_vlr) : m_special_vlr(special_vlr) {}

  std::optional<size_t> chunk_size() const {
    if (m_special_vlr.chunk_size == std::numeric_limits<uint32_t>::max()) {
      return std::nullopt;
    }
    return m_special_vlr.chunk_size;
  }

  template <typename T>
  std::span<T> decompress_chunk(std::span<std::byte> compressed_data,
                                std::span<T> decompressed_data) {
    Assert(m_chunk_table.has_value());

    std::vector<LAZEncoder> encoders;
    for (LAZItemRecord record : m_special_vlr.items_records) {
      switch (record.item_type) {
        case LAZItemType::Point10: {
          encoders.push_back(
              LASPointFormat0Encoder(*reinterpret_cast<LASPointFormat0*>(compressed_data.data())));
          compressed_data = compressed_data.subspan(sizeof(LASPointFormat0));
          break;
        }
        case LAZItemType::GPSTime11: {
          encoders.push_back(GPSTime11Encoder(*reinterpret_cast<GPSTime*>(compressed_data.data())));
          compressed_data = compressed_data.subspan(sizeof(GPSTime));
          break;
        }
        case LAZItemType::Byte: {
          std::vector<std::byte> last_bytes(record.item_size);
          std::copy_n(compressed_data.data(), record.item_size, last_bytes.begin());
          encoders.push_back(BytesEncoder(last_bytes));
          compressed_data = compressed_data.subspan(record.item_size);
          break;
        }
        default:
          Fail("Currently unsupported LAZ item type: ", (LAZItemType)record.item_type);
      }
    }

    PointerStreamBuffer compressed_buffer(compressed_data.data(), compressed_data.size());
    std::istream compressed_stream(&compressed_buffer);
    InStream compressed_in_stream(compressed_stream);
    std::vector<std::byte> next_bytes;
    for (size_t i = 0; i < decompressed_data.size(); i++) {
      for (LAZEncoder& encoder : encoders) {
        std::visit(
            [&compressed_in_stream, &decompressed_data, &i](auto&& encoder) {
              if (i > 0) encoder.decode(compressed_in_stream);

              if constexpr (is_copy_assignable<T, decltype(encoder.last_value())>()) {
                decompressed_data[i] = encoder.last_value();
              } else if constexpr (std::is_base_of_v<
                                       std::remove_reference_t<decltype(encoder.last_value())>,
                                       T>) {
                using RefType = std::add_lvalue_reference_t<
                    std::remove_const_t<std::remove_reference_t<decltype(encoder.last_value())>>>;
                (RefType) decompressed_data[i] = encoder.last_value();
              }
            },
            encoder);
      }
    }
    return decompressed_data;
  }
};

}  // namespace laspp
