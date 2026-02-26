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

#include <cstring>
#include <sstream>

#include "stream.hpp"

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
 public:
  LayeredInStreams(const LayeredInStreams&) = delete;
  LayeredInStreams& operator=(const LayeredInStreams&) = delete;
  LayeredInStreams(LayeredInStreams&&) = delete;
  LayeredInStreams& operator=(LayeredInStreams&&) = delete;

 private:
  // Each InStream reads directly from its slice of the in-memory compressed data —
  // no PointerStreamBuffer or std::istream wrapper needed.
  AlwaysConstructedArray<InStream, N_STREAMS> m_streams;
  std::array<bool, N_STREAMS> m_non_empty{};

  // Dummy buffer for empty layers (InStream requires at least 4 bytes to initialize).
  // This is only used for layers with size 0 or < 4, which should never be read from
  // (non_empty() check prevents access).
  static constexpr std::array<std::byte, 4> s_empty_layer_buffer = {std::byte{0}, std::byte{0},
                                                                    std::byte{0}, std::byte{0}};

 public:
  LayeredInStreams(std::span<std::byte>& layer_sizes, std::span<std::byte>& compressed_layer_data) {
    for (std::size_t i = 0; i < N_STREAMS; ++i) {
      std::uint32_t layer_size = 0;
      std::memcpy(&layer_size, layer_sizes.data(), sizeof(layer_size));
      m_non_empty[i] = layer_size > 0;
      layer_sizes = layer_sizes.subspan(sizeof(layer_size));

      // InStream requires at least 4 bytes to initialize. For empty or very small layers,
      // use a dummy buffer. These streams should never be read from (non_empty() check).
      if (layer_size >= 4) {
        m_streams.construct(i, compressed_layer_data.data(), static_cast<size_t>(layer_size));
        compressed_layer_data = compressed_layer_data.subspan(layer_size);
      } else {
        // Use dummy buffer for empty/small layers
        m_streams.construct(i, s_empty_layer_buffer.data(), s_empty_layer_buffer.size());
        // Still advance the compressed_data span even if we didn't use it
        compressed_layer_data = compressed_layer_data.subspan(layer_size);
      }
    }
  }

  bool non_empty(std::size_t i) const noexcept { return m_non_empty[i]; }

  InStream& operator[](std::size_t i) noexcept { return m_streams[i]; }
};

template <size_t N_STREAMS>
class LayeredOutStreams {
 public:
  LayeredOutStreams(const LayeredOutStreams&) = delete;
  LayeredOutStreams& operator=(const LayeredOutStreams&) = delete;
  LayeredOutStreams(LayeredOutStreams&&) = delete;
  LayeredOutStreams& operator=(LayeredOutStreams&&) = delete;

 private:
  std::array<std::stringstream, N_STREAMS> m_layer_stringstreams;
  AlwaysConstructedArray<OutStream, N_STREAMS> m_streams;

 public:
  LayeredOutStreams() {
    for (size_t i = 0; i < N_STREAMS; i++) {
      m_streams.construct(i, m_layer_stringstreams[i]);
    }
  }

  OutStream& operator[](size_t i) noexcept { return m_streams[i]; }

  std::array<uint32_t, N_STREAMS> layer_sizes() {
    std::array<uint32_t, N_STREAMS> sizes;
    for (size_t i = 0; i < N_STREAMS; i++) {
      m_streams[i].finalize();
      sizes[i] = static_cast<uint32_t>(m_layer_stringstreams[i].str().size());
    }
    return sizes;
  }

  std::stringstream cb() {
    std::stringstream combined;
    for (size_t i = 0; i < N_STREAMS; i++) {
      combined << std::move(m_layer_stringstreams[i]).rdbuf();
    }
    return combined;
  }

  std::stringstream combined_stream() {
    std::stringstream combined;
    auto sizes = layer_sizes();
    for (size_t i = 0; i < N_STREAMS; i++) {
      uint32_t layer_size = sizes[i];
      combined.write(reinterpret_cast<const char*>(&layer_size), sizeof(layer_size));
    }
    for (size_t i = 0; i < N_STREAMS; i++) {
      combined << std::move(m_layer_stringstreams[i]).rdbuf();
    }
    return combined;
  }
};

}  // namespace laspp
