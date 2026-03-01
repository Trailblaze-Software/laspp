/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
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
