/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __GNUC__
#define LASPP_PACKED __attribute__((packed))
#else
#define LASPP_PACKED
#endif

// Platform-specific memory prefetch macro
#ifdef _MSC_VER
// MSVC: Only use SSE prefetch on x86/x64 architectures
#if defined(_M_IX86) || defined(_M_X64)
// Include xmmintrin.h for SSE prefetch intrinsics
// This header provides _mm_prefetch and _MM_HINT_T0
#ifdef __has_include
#if __has_include(<xmmintrin.h>)
#include <xmmintrin.h>
#define LASPP_PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0)
#else
// Header not available: no-op fallback
#define LASPP_PREFETCH(addr) ((void)0)
#endif
#else
// Compiler doesn't support __has_include: include the header directly
// <xmmintrin.h> should be available on all MSVC x86/x64 configurations
#include <xmmintrin.h>
#define LASPP_PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0)
#endif
#else
// Non-x86/x64 architectures (e.g., ARM64): no-op
#define LASPP_PREFETCH(addr) ((void)0)
#endif
#else
// GCC/Clang: use builtin prefetch
#define LASPP_PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif
