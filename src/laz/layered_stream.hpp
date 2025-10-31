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

#pragma once

#include <cstring>

#include "stream.hpp"
#include "utilities/assert.hpp"

namespace laspp {

template <class T, std::size_t N>
class AlwaysConstructedArray {
  using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
  std::array<Storage, N> storage_{};

  static T* ptr(Storage& s) noexcept { return std::launder(reinterpret_cast<T*>(&s)); }
  static const T* ptr(const Storage& s) noexcept {
    return std::launder(reinterpret_cast<const T*>(&s));
  }

 public:
  AlwaysConstructedArray() = default;
  AlwaysConstructedArray(const AlwaysConstructedArray&) = delete;
  AlwaysConstructedArray& operator=(const AlwaysConstructedArray&) = delete;

  ~AlwaysConstructedArray() {
    for (std::size_t i = 0; i < N; ++i) std::destroy_at(ptr(storage_[i]));
  }

  template <class... Args>
  T& construct(std::size_t i, Args&&... args) {
    return *std::construct_at(ptr(storage_[i]), std::forward<Args>(args)...);
  }

  T& operator[](std::size_t i) noexcept { return *ptr(storage_[i]); }
  const T& operator[](std::size_t i) const noexcept { return *ptr(storage_[i]); }
};

template <std::size_t N_STREAMS>
class LayeredInStreams {
  AlwaysConstructedArray<PointerStreamBuffer, N_STREAMS> m_layer_stream_buffers;
  AlwaysConstructedArray<std::istream, N_STREAMS> m_layer_streams;
  AlwaysConstructedArray<InStream, N_STREAMS> m_streams;

 public:
  LayeredInStreams(std::span<std::byte>& layer_sizes, std::span<std::byte>& compressed_layer_data) {
    for (std::size_t i = 0; i < N_STREAMS; ++i) {
      std::uint32_t layer_size = 0;
      std::memcpy(&layer_size, layer_sizes.data(), sizeof(layer_size));
      layer_sizes = layer_sizes.subspan(sizeof(layer_size));
      m_layer_stream_buffers.construct(i, compressed_layer_data.data(), layer_size);
      m_layer_streams.construct(i, &m_layer_stream_buffers[i]);
      m_streams.construct(i, m_layer_streams[i]);
      compressed_layer_data = compressed_layer_data.subspan(layer_size);
    }
  }

  InStream& operator[](std::size_t i) noexcept { return m_streams[i]; }
};

template <size_t N_STREAMS>
class LayeredOutStreams {
  std::array<std::stringstream, N_STREAMS> m_layer_stringstreams;
  AlwaysConstructedArray<OutStream, N_STREAMS> m_streams;

 public:
  LayeredOutStreams() {
    for (size_t i = 0; i < N_STREAMS; i++) {
      m_streams.construct(i, m_layer_stringstreams[i]);
    }
  }

  OutStream& operator[](size_t i) noexcept { return m_streams[i]; }

  std::stringstream combined_stream() {
    std::stringstream combined;
    for (size_t i = 0; i < N_STREAMS; i++) {
      std::string layer_data = m_layer_stringstreams[i].str();
      uint32_t layer_size = static_cast<uint32_t>(layer_data.size());
      combined.write(reinterpret_cast<const char*>(&layer_size), sizeof(layer_size));
    }
    for (size_t i = 0; i < N_STREAMS; i++) {
      std::string layer_data = m_layer_stringstreams[i].str();  // TODO: avoid copy
      combined.write(layer_data.data(), layer_data.size());
    }
    return combined;
  }
};

}  // namespace laspp
