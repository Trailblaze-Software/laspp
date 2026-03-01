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
  const char* env_threads = nullptr;
#ifdef _WIN32
  // Use _dupenv_s on Windows to avoid deprecation warning
  char* env_threads_raw = nullptr;
  size_t len = 0;
  std::unique_ptr<char, decltype(&free)> guard(nullptr, &free);
  if (_dupenv_s(&env_threads_raw, &len, "LASPP_NUM_THREADS") == 0 && env_threads_raw != nullptr) {
    guard.reset(env_threads_raw);
    env_threads = env_threads_raw;
  }
#else
  env_threads = std::getenv("LASPP_NUM_THREADS");
#endif
  if (env_threads != nullptr) {
    constexpr long MAX_THREADS = 1024;
    char* end = nullptr;
    long num_threads = std::strtol(env_threads, &end, 10);
    // Check: valid conversion, no trailing chars, positive, and within bounds
    if (end != env_threads && *end == '\0' && num_threads > 0 && num_threads <= MAX_THREADS) {
      return static_cast<size_t>(num_threads);
    }
  }
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

    // Use shared_ptr to ensure atomics live long enough for all threads
    auto next_index = std::make_shared<std::atomic<size_t>>(begin);
    auto completed_count = std::make_shared<std::atomic<size_t>>(0);
    const size_t total_work = end - begin;

    // Wake all worker threads to grab work dynamically
    for (size_t i = 0; i < m_num_threads; ++i) {
      enqueue([next_index, end, func, completed_count] {
        while (true) {
          size_t idx = next_index->fetch_add(1);
          if (idx >= end) {
            break;
          }
          func(idx);
          completed_count->fetch_add(1);
        }
      });
    }

    // Wait for all work to complete
    while (completed_count->load() < total_work) {
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

// Global thread pool instance (lazy-initialized)
inline ThreadPool& get_thread_pool() {
  static std::once_flag init_flag;
  static std::unique_ptr<ThreadPool> pool;

  std::call_once(init_flag, []() { pool = std::make_unique<ThreadPool>(); });

  return *pool;
}

// Convenience function for parallel_for using the global thread pool
template <typename Func>
void parallel_for(size_t begin, size_t end, Func func) {
  get_thread_pool().parallel_for(begin, end, func);
}

}  // namespace utilities
}  // namespace laspp
