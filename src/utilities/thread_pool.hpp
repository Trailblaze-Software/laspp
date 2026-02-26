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
    char* end = nullptr;
    long num_threads = std::strtol(env_threads, &end, 10);
    if (end != env_threads && *end == '\0' && num_threads > 0) {
      return static_cast<size_t>(num_threads);
    }
  }
#else
  const char* env_threads = std::getenv("LASPP_NUM_THREADS");
  if (env_threads != nullptr) {
    char* end = nullptr;
    long num_threads = std::strtol(env_threads, &end, 10);
    if (end != env_threads && *end == '\0' && num_threads > 0) {
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
    std::atomic<size_t> completed_tasks{0};

    // Launch tasks
    for (size_t i = 0; i < num_tasks; ++i) {
      enqueue([&next_index, end, chunk_size, &func, &completed_tasks] {
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
        completed_tasks++;
      });
    }

    // Wait for all tasks to complete
    while (completed_tasks.load() < num_tasks) {
      std::this_thread::yield();
    }
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
inline ThreadPool& get_thread_pool() {
  static std::unique_ptr<ThreadPool> pool_ptr;
  static std::mutex pool_mutex;
  static size_t last_thread_count = 0;

  std::lock_guard<std::mutex> lock(pool_mutex);

  size_t current_thread_count = get_num_threads();

  // Recreate pool if thread count changed or if it doesn't exist
  if (!pool_ptr || last_thread_count != current_thread_count) {
    pool_ptr = std::make_unique<ThreadPool>(current_thread_count);
    last_thread_count = current_thread_count;
  }

  return *pool_ptr;
}

// Convenience function for parallel_for using the global thread pool
template <typename Func>
void parallel_for(size_t begin, size_t end, Func func) {
  get_thread_pool().parallel_for(begin, end, func);
}

}  // namespace utilities
}  // namespace laspp
