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

/*
 * benchmark.cpp
 *
 * Measures LAS++ read/write throughput at varying thread counts and compares
 * against the LASzip C API (single-threaded reference).  Results are written
 * as a JSON object to stdout so that the companion benchmark.py script can
 * aggregate them with laspy / PDAL / laz-rs measurements.
 *
 * Usage:
 *   benchmark --file <path.laz> [--threads 1,2,4,8] [--iterations 3]
 *             [--warmup 1] [--operation read|write|both]
 *             [--chunk-size 50000] [--no-laszip]
 */

// ── Windows compatibility ────────────────────────────────────────────────────
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

// ── Thread count control via LASPP_NUM_THREADS environment variable ─────────

// ── LASzip C API ─────────────────────────────────────────────────────────────
#include <laszip_api.h>

// ── LAZperf ───────────────────────────────────────────────────────────────────
#ifdef LASPP_HAS_LAZPERF
#include <lazperf/readers.hpp>
#endif

// Undefine the Windows byte typedef (rpcndr.h) that clashes with std::byte
#ifdef _WIN32
#ifdef byte
#undef byte
#endif
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <cstdlib>
#include <memory>
#endif

#include "las_point.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

// ── Timing ───────────────────────────────────────────────────────────────────

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

static double elapsed_since(std::chrono::time_point<Clock> t0) {
  return std::chrono::duration_cast<Seconds>(Clock::now() - t0).count();
}

// ── Thread control via environment variable ──────────────────────────────────

struct ThreadControl {
  std::string old_value;
  bool had_old_value;
  bool should_unset;

  explicit ThreadControl(int n_threads) : had_old_value(false), should_unset(false) {
#ifdef _WIN32
    // Use _dupenv_s on Windows to avoid deprecation warning
    char* old = nullptr;
    size_t len = 0;
    if (_dupenv_s(&old, &len, "LASPP_NUM_THREADS") == 0 && old != nullptr) {
      std::unique_ptr<char, decltype(&free)> guard(old, &free);
      old_value = old;
      had_old_value = true;
    }
#else
    const char* old = std::getenv("LASPP_NUM_THREADS");
    if (old != nullptr) {
      old_value = old;
      had_old_value = true;
    }
#endif
    if (n_threads > 0) {
      std::string new_value = std::to_string(n_threads);
#ifdef _WIN32
      _putenv_s("LASPP_NUM_THREADS", new_value.c_str());
#else
      setenv("LASPP_NUM_THREADS", new_value.c_str(), 1);
#endif
    } else if (n_threads == 0 && had_old_value) {
      // n_threads == 0 means use default, so unset the environment variable
#ifdef _WIN32
      _putenv_s("LASPP_NUM_THREADS", "");
#else
      unsetenv("LASPP_NUM_THREADS");
#endif
      should_unset = true;
    }
  }

  ~ThreadControl() {
    if (had_old_value) {
      // Restore the original value
#ifdef _WIN32
      _putenv_s("LASPP_NUM_THREADS", old_value.c_str());
#else
      setenv("LASPP_NUM_THREADS", old_value.c_str(), 1);
#endif
    } else if (should_unset) {
      // We unset it, so keep it unset (nothing to restore)
      // No action needed
    } else {
      // We set a new value, so unset it if it wasn't there before
#ifdef _WIN32
      char* current = nullptr;
      size_t len = 0;
      bool has_current =
          (_dupenv_s(&current, &len, "LASPP_NUM_THREADS") == 0 && current != nullptr);
      if (has_current) {
        std::unique_ptr<char, decltype(&free)> guard(current, &free);
        _putenv_s("LASPP_NUM_THREADS", "");
      }
#else
      const char* current = std::getenv("LASPP_NUM_THREADS");
      if (current != nullptr) {
        unsetenv("LASPP_NUM_THREADS");
      }
#endif
    }
  }
};

// ── Benchmark result ─────────────────────────────────────────────────────────

struct BenchResult {
  std::string tool;
  std::string operation;
  int threads;  ///< Requested thread count; 0 = OS default
  int iteration;
  double time_s;
};

// ── JSON helpers ─────────────────────────────────────────────────────────────

static std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else {
      out += c;
    }
  }
  return out;
}

// ── LAZperf: single-threaded read ────────────────────────────────────────────

#ifdef LASPP_HAS_LAZPERF
static double lazperf_read_once(const std::filesystem::path& path) {
  auto t0 = Clock::now();
  {
    lazperf::reader::named_file reader(path.string());
    uint64_t point_count = reader.pointCount();
    // Allocate buffer for largest point format (format 7 = 60 bytes)
    std::vector<char> point_buf(60);
    for (uint64_t i = 0; i < point_count; ++i) {
      reader.readPoint(point_buf.data());
    }
  }
  return elapsed_since(t0);
}
#endif

// ── LASzip C API: single-threaded read ───────────────────────────────────────

static double laszip_read_once(const std::filesystem::path& path) {
  laszip_POINTER reader = nullptr;
  if (laszip_create(&reader) != 0) {
    throw std::runtime_error("laszip_create failed");
  }

  laszip_BOOL is_compressed = 0;
  if (laszip_open_reader(reader, path.string().c_str(), &is_compressed) != 0) {
    laszip_destroy(reader);
    throw std::runtime_error("laszip_open_reader failed: " + path.string());
  }

  laszip_header* hdr = nullptr;
  laszip_get_header_pointer(reader, &hdr);
  laszip_U64 np = hdr->extended_number_of_point_records;
  if (np == 0) {
    np = static_cast<laszip_U64>(hdr->number_of_point_records);
  }

  laszip_point* pt = nullptr;
  laszip_get_point_pointer(reader, &pt);

  auto t0 = Clock::now();
  for (laszip_U64 i = 0; i < np; ++i) {
    if (laszip_read_point(reader) != 0) {
      laszip_close_reader(reader);
      laszip_destroy(reader);
      throw std::runtime_error("laszip_read_point failed at index " + std::to_string(i));
    }
  }
  double elapsed = elapsed_since(t0);

  laszip_close_reader(reader);
  laszip_destroy(reader);
  return elapsed;
}

// ── LAS++ read ────────────────────────────────────────────────────────────────

template <typename PointType>
static double laspp_read_once(const std::filesystem::path& path, int n_threads) {
  ThreadControl ctrl(n_threads);
  auto t0 = Clock::now();
  {
    std::ifstream ifs(path, std::ios::binary);
    LASReader reader(ifs);
    std::vector<PointType> points(reader.num_points());
    reader.read_chunks<PointType>(points, {0, reader.num_chunks()});
  }
  return elapsed_since(t0);
}

// ── Point-format helpers ──────────────────────────────────────────────────────

template <typename T>
static uint8_t get_point_format_number() {
  if constexpr (std::is_same_v<T, LASPointFormat0>) {
    return 0;
  } else if constexpr (std::is_same_v<T, LASPointFormat1>) {
    return 1;
  } else if constexpr (std::is_same_v<T, LASPointFormat2>) {
    return 2;
  } else if constexpr (std::is_same_v<T, LASPointFormat3>) {
    return 3;
  } else if constexpr (std::is_same_v<T, LASPointFormat6>) {
    return 6;
  } else if constexpr (std::is_same_v<T, LASPointFormat7>) {
    return 7;
  } else {
    UNREACHABLE();
  }
}

// ── LAS++ write ───────────────────────────────────────────────────────────────

template <typename PointType>
static double laspp_write_once(const std::vector<PointType>& points,
                               const std::filesystem::path& tmp_path, int n_threads,
                               std::optional<std::size_t> chunk_size) {
  ThreadControl ctrl(n_threads);
  uint8_t fmt = get_point_format_number<PointType>();
  // Always write compressed LAZ output
  uint8_t compressed_fmt = static_cast<uint8_t>(fmt | uint8_t{0x80u});

  auto t0 = Clock::now();
  {
    std::fstream ofs(tmp_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
      throw std::runtime_error("Cannot open temp file: " + tmp_path.string());
    }
    LASWriter writer(ofs, compressed_fmt);
    writer.write_points<PointType>(std::span<const PointType>(points), chunk_size);
    // LASWriter destructor flushes chunk table + header – included in timing
  }
  return elapsed_since(t0);
}

// ── Per-format benchmark runner ────────────────────────────────────────────────
//
// Runs all requested operations (laspp read, laspp write, laszip read, lazperf read) for
// the concrete PointType.  Results are appended to `results`.

template <typename PointType>
static void run_benchmark(const std::filesystem::path& path, bool do_read, bool do_write,
                          bool do_laszip, bool do_lazperf, const std::vector<int>& thread_counts,
                          int warmup, int iterations, std::optional<std::size_t> chunk_size,
                          std::vector<BenchResult>& results) {
  (void)do_lazperf;  // May be unused if LASPP_HAS_LAZPERF is not defined
  // Pre-read points into memory for the write benchmark (not timed).
  std::vector<PointType> cached_points;
  if (do_write) {
    std::ifstream ifs(path, std::ios::binary);
    LASReader rdr(ifs);
    cached_points.resize(rdr.num_points());
    rdr.read_chunks<PointType>(cached_points, {0, rdr.num_chunks()});
  }

  auto tmp_path = std::filesystem::temp_directory_path() / "laspp_benchmark_tmp.laz";

  for (int n_threads : thread_counts) {
    // ── laspp read ──────────────────────────────────────────────────────────
    if (do_read) {
      for (int w = 0; w < warmup; ++w) {
        laspp_read_once<PointType>(path, n_threads);
      }
      for (int it = 0; it < iterations; ++it) {
        double t = laspp_read_once<PointType>(path, n_threads);
        results.push_back({"laspp", "read", n_threads, it, t});
      }
    }

    // ── laspp write ─────────────────────────────────────────────────────────
    if (do_write && !cached_points.empty()) {
      for (int w = 0; w < warmup; ++w) {
        laspp_write_once<PointType>(cached_points, tmp_path, n_threads, chunk_size);
      }
      for (int it = 0; it < iterations; ++it) {
        double t = laspp_write_once<PointType>(cached_points, tmp_path, n_threads, chunk_size);
        results.push_back({"laspp", "write", n_threads, it, t});
      }
    }
  }

  // Clean up temp file
  if (do_write && std::filesystem::exists(tmp_path)) {
    std::filesystem::remove(tmp_path);
  }

  // ── laszip read (single-threaded reference) ─────────────────────────────
  if (do_laszip) {
    for (int w = 0; w < warmup; ++w) {
      laszip_read_once(path);
    }
    for (int it = 0; it < iterations; ++it) {
      double t = laszip_read_once(path);
      results.push_back({"laszip", "read", 1, it, t});
    }
  }

  // ── lazperf read (single-threaded) ──────────────────────────────────────
#ifdef LASPP_HAS_LAZPERF
  if (do_lazperf && do_read) {
    for (int w = 0; w < warmup; ++w) {
      lazperf_read_once(path);
    }
    for (int it = 0; it < iterations; ++it) {
      double t = lazperf_read_once(path);
      results.push_back({"lazperf", "read", 1, it, t});
    }
  }
#endif
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
  // ── Argument parsing ──────────────────────────────────────────────────────

  std::string file_str;
  std::vector<int> thread_counts;
  int warmup = 1;
  int iterations = 3;
  bool do_read = true;
  bool do_write = false;
  bool do_laszip = true;
  bool do_lazperf = true;
  std::optional<std::size_t> chunk_size;  // default: single chunk per write

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);

    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for argument: " + arg);
      }
      return std::string(argv[++i]);
    };

    if (arg == "--file" || arg == "-f") {
      file_str = next();
    } else if (arg == "--threads" || arg == "-t") {
      std::string val = next();
      std::istringstream ss(val);
      std::string tok;
      while (std::getline(ss, tok, ',')) {
        thread_counts.push_back(std::stoi(tok));
      }
    } else if (arg == "--iterations" || arg == "-n") {
      iterations = std::stoi(next());
    } else if (arg == "--warmup" || arg == "-w") {
      warmup = std::stoi(next());
    } else if (arg == "--operation") {
      std::string op = next();
      do_read = (op == "read" || op == "both");
      do_write = (op == "write" || op == "both");
    } else if (arg == "--chunk-size") {
      chunk_size = static_cast<std::size_t>(std::stoull(next()));
    } else if (arg == "--no-laszip") {
      do_laszip = false;
    } else if (arg == "--no-lazperf") {
      do_lazperf = false;
    } else if (arg == "--help" || arg == "-h") {
      std::cerr << "Usage: " << argv[0]
                << " --file <path.laz>\n"
                   "       [--threads 1,2,4,8]    comma-separated thread counts\n"
                   "       [--iterations 3]        timed iterations per config\n"
                   "       [--warmup 1]            warm-up iterations (untimed)\n"
                   "       [--operation read|write|both]\n"
                   "       [--chunk-size 50000]    points per LAZ chunk (write)\n"
                   "       [--no-laszip]           skip the laszip reference\n"
                   "       [--no-lazperf]          skip the lazperf reference\n";
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }

  if (file_str.empty()) {
    std::cerr << "Error: --file is required\n";
    return 1;
  }

  std::filesystem::path path(file_str);
  if (!std::filesystem::exists(path)) {
    std::cerr << "Error: file not found: " << file_str << "\n";
    return 1;
  }

  // Default thread-count sweep: powers of 2 up to hardware_concurrency
  if (thread_counts.empty()) {
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    for (int t = 1; t <= hw; t *= 2) {
      thread_counts.push_back(t);
    }
    if (!thread_counts.empty() && thread_counts.back() != hw) {
      thread_counts.push_back(hw);
    }
  }

  // Default chunk size for write benchmark (50 000 pts = standard LAZ chunk)
  if (do_write && !chunk_size.has_value()) {
    chunk_size = std::size_t{50000};
  }

  // ── Probe file header ─────────────────────────────────────────────────────

  std::ifstream probe(path, std::ios::binary);
  if (!probe.is_open()) {
    std::cerr << "Error: cannot open: " << file_str << "\n";
    return 1;
  }
  LASReader probe_reader(probe);
  uint8_t point_format_byte = probe_reader.header().point_format();
  uint8_t base_format = static_cast<uint8_t>(point_format_byte & 0x7Fu);
  bool is_compressed = (point_format_byte & 0x80u) != 0u;
  std::size_t num_points = probe_reader.num_points();
  probe.close();

  std::uintmax_t file_size_bytes = std::filesystem::file_size(path);
  int hw_threads = static_cast<int>(std::thread::hardware_concurrency());

  // ── Run benchmarks ────────────────────────────────────────────────────────

  std::vector<BenchResult> results;

  try {
    switch (base_format) {
      case 0:
        run_benchmark<LASPointFormat0>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      case 1:
        run_benchmark<LASPointFormat1>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      case 2:
        run_benchmark<LASPointFormat2>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      case 3:
        run_benchmark<LASPointFormat3>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      case 4:
      case 5:
        std::cerr << "Error: point formats 4 and 5 (wave packet) are not yet "
                     "supported\n";
        return 1;
      case 6:
        run_benchmark<LASPointFormat6>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      case 7:
        run_benchmark<LASPointFormat7>(path, do_read, do_write, do_laszip, do_lazperf,
                                       thread_counts, warmup, iterations, chunk_size, results);
        break;
      default:
        std::cerr << "Error: unsupported point format " << static_cast<int>(base_format) << "\n";
        return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "Benchmark error: " << e.what() << "\n";
    return 1;
  }

  // ── JSON output ───────────────────────────────────────────────────────────

  std::cout << "{\n";

  // Platform / capabilities
  std::cout << "  \"platform\": \"";
#if defined(_WIN32)
  std::cout << "windows";
#elif defined(__APPLE__)
  std::cout << "macos";
#else
  std::cout << "linux";
#endif
  std::cout << "\",\n";

  std::cout << "  \"has_thread_control\": true";  // Always available via LASPP_NUM_THREADS
  std::cout << ",\n";

  std::cout << "  \"hardware_threads\": " << hw_threads << ",\n";
  std::cout << "  \"file\": \"" << json_escape(path.string()) << "\",\n";
  std::cout << "  \"num_points\": " << num_points << ",\n";
  std::cout << "  \"file_size_bytes\": " << file_size_bytes << ",\n";
  std::cout << "  \"point_format\": " << static_cast<int>(base_format) << ",\n";
  std::cout << "  \"is_compressed\": " << (is_compressed ? "true" : "false") << ",\n";

  std::cout << "  \"results\": [\n";
  for (std::size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    std::cout << "    {"
              << "\"tool\": \"" << r.tool << "\", "
              << "\"operation\": \"" << r.operation << "\", "
              << "\"threads\": " << r.threads << ", "
              << "\"iteration\": " << r.iteration << ", "
              << "\"time_s\": " << r.time_s << "}";
    if (i + 1 < results.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }
  std::cout << "  ]\n";
  std::cout << "}\n";

  return 0;
}
