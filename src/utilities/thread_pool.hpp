/*
 * SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "env.hpp"

namespace laspp {
namespace utilities {

// Get the number of threads to use from LASPP_NUM_THREADS environment variable,
// or return hardware_concurrency() if not set or invalid.
inline size_t get_num_threads() {
  auto env_threads = get_env("LASPP_NUM_THREADS");
  if (env_threads.has_value()) {
    constexpr long MAX_THREADS = 1024;
    const char* str = env_threads->c_str();
    char* end = nullptr;
    long num_threads = std::strtol(str, &end, 10);
    // Check: valid conversion, no trailing chars, positive, and within bounds
    if (end != str && *end == '\0' && num_threads > 0 && num_threads <= MAX_THREADS) {
      return static_cast<size_t>(num_threads);
    }
  }
  return std::max(size_t{1}, static_cast<size_t>(std::thread::hardware_concurrency()));
}

// Simple thread pool for parallel execution
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads = get_num_threads())
      : m_max_threads(std::max(size_t{1}, num_threads)), m_stop(false) {
    // Don't create threads yet - create them lazily on demand
    m_workers.reserve(m_max_threads);
  }

  // Ensure we have at least n_threads worker threads created
  void ensure_threads(size_t n_threads) {
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    while (m_workers.size() < n_threads && m_workers.size() < m_max_threads) {
      m_workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> task_lock(m_queue_mutex);
            m_condition.wait(task_lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_stop && m_tasks.empty()) {
              return;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop();
          }
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      m_stop = true;
    }
    m_condition.notify_all();
    for (std::thread& worker : m_workers) {
      worker.join();
    }
  }

  // Non-copyable, non-movable
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  // Execute a function for each index in [begin, end)
  // chunk_size: number of indices to process per atomic increment (default: 1)
  template <typename Func>
  void parallel_for(size_t begin, size_t end, Func func, size_t chunk_size = 1) {
    if (begin >= end) {
      return;
    }

    size_t requested_threads = get_num_threads();
    ensure_threads(requested_threads);
    size_t active_threads = std::min(requested_threads, m_max_threads);

    const size_t total_work = end - begin;
    const size_t num_chunks = (total_work + chunk_size - 1) / chunk_size;  // Ceiling division

    // Use latch to block caller until all work is complete
    auto completion_latch = std::make_shared<std::latch>(num_chunks);

    // Use shared_ptr to ensure atomics live long enough for all threads
    auto next_chunk = std::make_shared<std::atomic<size_t>>(0);

    for (size_t i = 0; i < active_threads; ++i) {
      enqueue([next_chunk, num_chunks, begin, end, chunk_size, func, completion_latch]() mutable {
        while (true) {
          size_t chunk_idx = next_chunk->fetch_add(1);
          if (chunk_idx >= num_chunks) {
            break;
          }

          // Process all indices in this chunk
          size_t chunk_begin = begin + chunk_idx * chunk_size;
          size_t chunk_end = std::min(chunk_begin + chunk_size, end);
          for (size_t idx = chunk_begin; idx < chunk_end; ++idx) {
            func(idx);
          }

          // Signal completion of this chunk (reduces atomic contention)
          completion_latch->count_down(1);
        }
      });
    }

    // Block until all chunks are complete
    completion_latch->wait();
  }

 private:
  void enqueue(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      m_tasks.emplace(std::move(task));
    }
    m_condition.notify_one();
  }

  size_t m_max_threads;
  std::vector<std::thread> m_workers;
  std::queue<std::function<void()>> m_tasks;
  std::mutex m_queue_mutex;
  std::condition_variable m_condition;
  std::atomic<bool> m_stop;
};

// Global thread pool instance (lazy-initialized)
inline ThreadPool& get_thread_pool() {
  static std::once_flag init_flag;
  static std::unique_ptr<ThreadPool> pool;

  std::call_once(init_flag, []() {
    size_t max_threads =
        std::max(size_t{1}, static_cast<size_t>(std::thread::hardware_concurrency()));
    pool = std::make_unique<ThreadPool>(max_threads);
  });

  return *pool;
}

// Convenience function for parallel_for using the global thread pool
template <typename Func>
void parallel_for(size_t begin, size_t end, Func func, size_t chunk_size = 1) {
  get_thread_pool().parallel_for(begin, end, func, chunk_size);
}

}  // namespace utilities
}  // namespace laspp
