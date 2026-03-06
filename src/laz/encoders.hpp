/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <variant>

#include "laz/byte_encoder.hpp"
#include "laz/gpstime11_encoder.hpp"
#include "laz/point10_encoder.hpp"
#include "laz/point14_encoder.hpp"
#include "laz/rgb12_encoder.hpp"
#include "laz/rgb14_encoder.hpp"

namespace laspp {

// Each encoder lives on the heap via unique_ptr so the variant is pointer-sized
// regardless of which (potentially large) encoder type is active.
typedef std::variant<std::unique_ptr<LASPointFormat0Encoder>, std::unique_ptr<GPSTime11Encoder>,
                     std::unique_ptr<RGB12Encoder>, std::unique_ptr<RGB14Encoder>,
                     std::unique_ptr<BytesEncoder>, std::unique_ptr<LASPointFormat6Encoder>>
    LAZEncoder;
}  // namespace laspp
