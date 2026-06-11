/*
 * SPDX-FileCopyrightText: (c) 2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 *
 * Tests for LASReader::set_progress_callback — verifies the callback fires
 * correctly with both single-chunk and multi-chunk files, across memory-mapped
 * and stream-based I/O paths, and with multiple threads.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include "las_header.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "utilities/assert.hpp"

using namespace laspp;

namespace {

class TempFile {
 public:
  explicit TempFile(const std::string& prefix) {
    auto base_dir = std::filesystem::temp_directory_path() / "laspp_tests";
    std::filesystem::create_directories(base_dir);
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = base_dir / (prefix + "_" + std::to_string(timestamp) + ".las");
  }
  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void set_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  if (value != nullptr && *value != '\0') {
    setenv(name, value, 1);
  } else {
    unsetenv(name);
  }
#endif
}

// Collect fractions from the progress callback in a thread-safe way,
// then verify they form the complete set {1/N, 2/N, ..., N/N}.
void verify_fractions(const std::vector<double>& fractions, size_t expected_count) {
  LASPP_ASSERT_EQ(fractions.size(), expected_count);
  for (size_t i = 0; i < expected_count; ++i) {
    double expected = static_cast<double>(i + 1) / static_cast<double>(expected_count);
    LASPP_ASSERT(std::abs(fractions[i] - expected) < 1e-9);
  }
}

// Write a multi-chunk LAZ file by calling write_points in separate batches.
// Each batch becomes its own LAZ chunk, giving us deterministic chunk counts.
void write_multi_chunk_laz(const std::filesystem::path& path, size_t num_batches) {
  std::fstream ofs(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  LASWriter writer(ofs, 128);  // Format 0 + LAZ compression

  const size_t total_points = num_batches * 30;
  std::vector<LASPointFormat0> points(total_points);
  for (size_t i = 0; i < total_points; ++i) {
    points[i].x = static_cast<int32_t>(i);
  }

  for (size_t b = 0; b < num_batches; ++b) {
    writer.write_points(std::span<const LASPointFormat0>(points).subspan(b * 30, 30));
  }
}

// Write a multi-chunk LAZ to a stringstream for stream-based tests
void write_multi_chunk_laz_stream(std::stringstream& stream, size_t num_batches) {
  LASWriter writer(stream, 128);

  const size_t total_points = num_batches * 30;
  std::vector<LASPointFormat0> points(total_points);
  for (size_t i = 0; i < total_points; ++i) {
    points[i].x = static_cast<int32_t>(i);
  }

  for (size_t b = 0; b < num_batches; ++b) {
    writer.write_points(std::span<const LASPointFormat0>(points).subspan(b * 30, 30));
  }
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  // ── Test 1: Non-LAZ (single chunk) calls progress with exactly 1.0 ──
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 0, 0);
      std::vector<LASPointFormat0> points(50);
      for (size_t i = 0; i < 50; ++i) points[i].x = static_cast<int32_t>(i);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    LASReader reader(stream);
    LASPP_ASSERT_EQ(reader.num_chunks(), 1u);

    std::vector<double> fractions;
    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(50);
    reader.read_chunks<LASPointFormat0>(points, {0, 1});

    LASPP_ASSERT_EQ(fractions.size(), 1u);
    LASPP_ASSERT_EQ(fractions[0], 1.0);
  }

  // ── Test 2: LAZ multi-chunk read_chunks — monotonic and complete ──
  {
    std::stringstream stream;
    write_multi_chunk_laz_stream(stream, 4);

    LASReader reader(stream);
    size_t N = reader.num_chunks();
    LASPP_ASSERT_GE(N, 2u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(120);
    reader.read_chunks<LASPointFormat0>(points, {0, N});

    // Callbacks are serialised → must arrive strictly increasing
    LASPP_ASSERT_EQ(fractions.size(), N);
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    // Also verify the complete set
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, N);
  }

  // ── Test 3: read_chunks_list — monotonic and complete ──
  {
    std::stringstream stream;
    write_multi_chunk_laz_stream(stream, 4);

    LASReader reader(stream);
    LASPP_ASSERT_GE(reader.num_chunks(), 3u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    // Read only chunks 0 and 2 (skip chunks 1 and 3)
    std::vector<size_t> chunk_list = {0, 2};
    const auto& ppc = reader.points_per_chunk();
    size_t total_pts = ppc[0] + ppc[2];
    std::vector<LASPointFormat0> points(total_pts);
    reader.read_chunks_list<LASPointFormat0>(points, chunk_list);

    LASPP_ASSERT_EQ(fractions.size(), chunk_list.size());
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, chunk_list.size());
  }

  // ── Test 4: No callback set — no crash ──
  {
    std::stringstream stream;
    {
      LASWriter writer(stream, 128, 0);
      std::vector<LASPointFormat0> points(50);
      for (size_t i = 0; i < 50; ++i) points[i].x = static_cast<int32_t>(i);
      writer.write_points(std::span<const LASPointFormat0>(points));
    }

    LASReader reader(stream);
    // Deliberately no set_progress_callback — must not crash
    std::vector<LASPointFormat0> points(50);
    reader.read_chunks<LASPointFormat0>(points, {0, reader.num_chunks()});

    // Also exercise read_chunks_list without callback
    reader.set_progress_callback({});  // explicit empty callback
    reader.read_chunks_list<LASPointFormat0>(points, {0});
  }

  // ── Test 5: Memory-mapped I/O path — monotonic and complete ──
  {
    TempFile temp_file("test_progress_mmap");
    write_multi_chunk_laz(temp_file.path(), 4);

    LASReader reader(temp_file.path());
    LASPP_ASSERT(reader.is_using_memory_mapping());

    size_t N = reader.num_chunks();
    LASPP_ASSERT_GE(N, 2u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(120);
    reader.read_chunks<LASPointFormat0>(points, {0, N});

    LASPP_ASSERT_EQ(fractions.size(), N);
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, N);
  }

  // ── Test 6: Stream-based I/O path (ifstream, not mmap) ──
  {
    TempFile temp_file("test_progress_stream");
    write_multi_chunk_laz(temp_file.path(), 4);

    std::ifstream ifs(temp_file.path(), std::ios::binary);
    LASReader reader(ifs);
    LASPP_ASSERT(!reader.is_using_memory_mapping());

    size_t N = reader.num_chunks();
    LASPP_ASSERT_GE(N, 2u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(120);
    reader.read_chunks<LASPointFormat0>(points, {0, N});

    LASPP_ASSERT_EQ(fractions.size(), N);
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, N);
  }

  // ── Test 7: Monotonic with multiple threads (LASPP_NUM_THREADS=4) ──
  //            Callbacks are serialised by LASReader's internal mutex, so
  //            fractions must arrive in strictly increasing order even when
  //            multiple worker threads are decompressing chunks.
  {
    TempFile temp_file("test_progress_mt");
    write_multi_chunk_laz(temp_file.path(), 5);  // 5 chunks for parallelism

    set_env("LASPP_NUM_THREADS", "4");

    LASReader reader(temp_file.path());
    size_t N = reader.num_chunks();
    LASPP_ASSERT_GE(N, 3u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(150);
    reader.read_chunks<LASPointFormat0>(points, {0, N});

    set_env("LASPP_NUM_THREADS", nullptr);

    // Fractions must arrive strictly increasing (guaranteed by the mutex)
    LASPP_ASSERT_EQ(fractions.size(), N);
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    // Also verify the complete set
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, N);
  }

  // ── Test 8: Monotonic with single thread (LASPP_NUM_THREADS=1) ──
  {
    TempFile temp_file("test_progress_st");
    write_multi_chunk_laz(temp_file.path(), 4);

    set_env("LASPP_NUM_THREADS", "1");

    LASReader reader(temp_file.path());
    size_t N = reader.num_chunks();
    LASPP_ASSERT_GE(N, 2u);

    std::vector<double> fractions;
    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<LASPointFormat0> points(120);
    reader.read_chunks<LASPointFormat0>(points, {0, N});

    set_env("LASPP_NUM_THREADS", nullptr);

    // Fractions must arrive strictly increasing
    LASPP_ASSERT_EQ(fractions.size(), N);
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, N);
  }

  // ── Test 9: read_chunks_list with memory-mapped path ──
  {
    TempFile temp_file("test_progress_mmap_list");
    write_multi_chunk_laz(temp_file.path(), 4);

    LASReader reader(temp_file.path());
    LASPP_ASSERT(reader.is_using_memory_mapping());
    LASPP_ASSERT_GE(reader.num_chunks(), 3u);

    std::vector<double> fractions;

    reader.set_progress_callback([&](double f) { fractions.push_back(f); });

    std::vector<size_t> chunk_list = {0, 3};
    const auto& ppc = reader.points_per_chunk();
    size_t total_pts = ppc[0] + ppc[3];
    std::vector<LASPointFormat0> points(total_pts);
    reader.read_chunks_list<LASPointFormat0>(points, chunk_list);

    LASPP_ASSERT_EQ(fractions.size(), chunk_list.size());
    for (size_t i = 1; i < fractions.size(); ++i) {
      LASPP_ASSERT(fractions[i] > fractions[i - 1]);
    }
    std::sort(fractions.begin(), fractions.end());
    verify_fractions(fractions, chunk_list.size());
  }

  return 0;
}
