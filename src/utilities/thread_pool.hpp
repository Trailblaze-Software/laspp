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
#ifdef _WIN32
#include <cstring>
#include <memory>
#endif

namespace laspp {
namespace utilities {

// Get the number of threads to use from LASPP_NUM_THREADS environment variable,
// or return hardware_concurrency() if not set or invalid.
inline size_t get_num_threads() {
#ifdef _WIN32
  // Use _dupenv_s on Windows to avoid deprecation warning
  char* env_threads = nullptr;
  size_t len = 0;
  if (_dupenv_s(&env_threads, &len, "LASPP_NUM_THREADS") == 0 && env_threads != nullptr) {
    std::unique_ptr<char, decltype(&free)> guard(env_threads, &free);
    constexpr long MAX_THREADS = 1024;
    char* end = nullptr;
    long num_threads = std::strtol(env_threads, &end, 10);
    // Check: valid conversion, no trailing chars, positive, and within bounds
    if (end != env_threads && *end == '\0' && num_threads > 0 && num_threads <= MAX_THREADS) {
      return static_cast<size_t>(num_threads);
    }
  }
#else
  const char* env_threads = std::getenv("LASPP_NUM_THREADS");
  if (env_threads != nullptr) {
    constexpr long MAX_THREADS = 1024;
    char* end = nullptr;
    long num_threads = std::strtol(env_threads, &end, 10);
    // Check: valid conversion, no trailing chars, positive, and within bounds
    if (end != env_threads && *end == '\0' && num_threads > 0 && num_threads <= MAX_THREADS) {
      return static_cast<size_t>(num_threads);
    }
  }
#endif
  return std::max(size_t{1}, static_cast<size_t>(std::thread::hardware_concurrency()));
}

// Simple thread pool for parallel execution
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads = get_num_threads())
      : m_num_threads(std::max(size_t{1}, num_threads)), m_stop(false) {
    m_workers.reserve(m_num_threads);
    for (size_t i = 0; i < m_num_threads; ++i) {
      m_workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
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
  template <typename Func>
  void parallel_for(size_t begin, size_t end, Func func) {
    if (begin >= end) {
      return;
    }

    const size_t range = end - begin;
    const size_t num_tasks = std::min(m_num_threads, range);
    const size_t chunk_size = std::max(size_t{1}, range / num_tasks);
    std::atomic<size_t> next_index{begin};

    std::latch completion_latch{static_cast<ptrdiff_t>(num_tasks)};

    // Launch tasks
    for (size_t i = 0; i < num_tasks; ++i) {
      enqueue([&next_index, end, chunk_size, &func, &completion_latch] {
        while (true) {
          size_t idx = next_index.fetch_add(chunk_size);
          if (idx >= end) {
            break;
          }
          size_t end_idx = std::min(idx + chunk_size, end);
          for (size_t j = idx; j < end_idx; ++j) {
            func(j);
          }
        }
        completion_latch.count_down();
      });
    }

    completion_latch.wait();
  }

 private:
  void enqueue(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      m_tasks.emplace(std::move(task));
    }
    m_condition.notify_one();
  }

  size_t m_num_threads;
  std::vector<std::thread> m_workers;
  std::queue<std::function<void()>> m_tasks;
  std::mutex m_queue_mutex;
  std::condition_variable m_condition;
  std::atomic<bool> m_stop;
};

// Global thread pool instance (lazy-initialized, respects LASPP_NUM_THREADS changes)
// Uses lock-free fast path check + mutex for recreation when thread count changes
inline ThreadPool& get_thread_pool() {
  // Fast path: lock-free check if pool exists and thread count matches
  static std::atomic<ThreadPool*> pool_ptr{nullptr};
  static std::atomic<size_t> cached_thread_count{0};
  static std::mutex pool_mutex;                   // Only used for initialization/recreation
  static std::unique_ptr<ThreadPool> owned_pool;  // Owns the pool lifetime

  size_t current_thread_count = get_num_threads();
  ThreadPool* current_pool = pool_ptr.load(std::memory_order_acquire);
  size_t cached_count = cached_thread_count.load(std::memory_order_acquire);

  // Fast path: pool exists and thread count matches (no lock needed)
  if (current_pool != nullptr && cached_count == current_thread_count) {
    return *current_pool;
  }

  // Slow path: need to create or recreate pool (acquire lock)
  std::lock_guard<std::mutex> lock(pool_mutex);

  // Double-check after acquiring lock (another thread might have created it)
  current_pool = pool_ptr.load(std::memory_order_acquire);
  cached_count = cached_thread_count.load(std::memory_order_acquire);

  if (current_pool != nullptr && cached_count == current_thread_count) {
    return *current_pool;
  }

  // Create new pool (old one will be destroyed automatically via unique_ptr)
  owned_pool = std::make_unique<ThreadPool>(current_thread_count);
  current_pool = owned_pool.get();

  // Publish with release semantics (ensures pool is fully constructed)
  pool_ptr.store(current_pool, std::memory_order_release);
  cached_thread_count.store(current_thread_count, std::memory_order_release);

  return *current_pool;
}

// Convenience function for parallel_for using the global thread pool
template <typename Func>
void parallel_for(size_t begin, size_t end, Func func) {
  get_thread_pool().parallel_for(begin, end, func);
}

}  // namespace utilities
}  // namespace laspp
