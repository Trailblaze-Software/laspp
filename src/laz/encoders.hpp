/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "laz/byte14_encoder.hpp"
#include "laz/byte_encoder.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/raw_bytes_encoder.hpp"
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"
#include "laz/rgbnir14_encoder.hpp"

namespace laspp {

// Each encoder lives on the heap via unique_ptr so the variant is pointer-sized
// regardless of which (potentially large) encoder type is active.
// Byte14 items expand to one Byte14Encoder per extra-byte slot (stored as a vector)
// so each encoder is paired with exactly one LayeredInStreams/OutStreams<1>.
typedef std::variant<std::unique_ptr<LASPointFormat0Encoder>, std::unique_ptr<GPSTime11Encoder>,
                     std::unique_ptr<RGB12Encoder>, std::unique_ptr<RGB14Encoder>,
                     std::unique_ptr<BytesEncoder>, std::unique_ptr<LASPointFormat6EncoderV3>,
                     std::unique_ptr<LASPointFormat6EncoderV4>, std::unique_ptr<RawBytesEncoder>,
                     std::unique_ptr<RGBNIR14Encoder>, std::vector<Byte14Encoder>>
    LAZEncoder;

}  // namespace laspp
