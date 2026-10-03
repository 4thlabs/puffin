//  ____         __  __ _
// |  _ \ _   _ / _|/ _(_)_ __
// | |_) | | | | |_| |_| | '_  |
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|
//
// BSD 3-Clause License

// Copyright (c) 2025, Thomas Gourgues (thomas.gourgues@gmail.com)
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.

// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.

// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef PUFFIN_ASYNC_THREAD_EXECUTOR_HPP
#define PUFFIN_ASYNC_THREAD_EXECUTOR_HPP

#include <puffin/async/executor.hpp>
#include <puffin/async/schedule.hpp>

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <deque>
#include <mutex>
#include <thread>

namespace puffin {
namespace async {

/**
 * @brief A basic single thread executor, resuming coroutines in posting order.
 *
 * run() pumps on the calling thread (or on a thread it owns with run(false)) until stop().
 * Coroutines still queued when it stops are not resumed.
 */
class thread_executor final {
public:
  thread_executor() = default;

  thread_executor(const thread_executor&) = delete;
  thread_executor& operator=(const thread_executor&) = delete;

  ~thread_executor() { wait(); }

  void post(std::coroutine_handle<> h)
  {
    // Notify under the lock: once h is queued, the executor may be destroyed as soon as the lock
    // is released
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(h);
    cv_.notify_one();
  }

  /**
   * @brief Moves the awaiting coroutine to this executor: co_await executor.schedule();
   */
  schedule_on schedule() { return schedule_on { *this }; }

  /**
   * @brief Runs the loop, blocking on the calling thread or on an owned thread
   */
  void run(bool blocking = true)
  {
    if (blocking)
      pump();
    else
      thread_ = std::thread(&thread_executor::pump, this);
  }

  /**
   * @brief Asks the loop to return once the current coroutine is suspended
   */
  void stop()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    cv_.notify_all();
  }

  /**
   * @brief Allows run() to be called again after a stop()
   */
  void restart()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
  }

  /**
   * @brief Stops and joins the owned thread, if any
   */
  void wait()
  {
    stop();

    if (!thread_.joinable())
      return;

    if (thread_.get_id() == std::this_thread::get_id())
      thread_.detach();
    else
      thread_.join();
  }

  bool running_in_this_thread() const noexcept { return running_thread_ == std::this_thread::get_id(); }

private:
  void pump()
  {
    running_thread_ = std::this_thread::get_id();

    while (true) {
      std::coroutine_handle<> h;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return stopped_ || !queue_.empty(); });

        if (stopped_)
          break;

        h = queue_.front();
        queue_.pop_front();
      }

      h.resume();
    }

    running_thread_ = std::thread::id {};
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::coroutine_handle<>> queue_;
  bool stopped_ = false;

  std::thread thread_;
  std::atomic<std::thread::id> running_thread_ {};
};

/**
 * @brief Resumes coroutines inline, in post(). Mostly useful for tests.
 */
class inline_executor final {
public:
  void post(std::coroutine_handle<> h) const { h.resume(); }

  friend bool operator==(const inline_executor&, const inline_executor&) noexcept { return true; }
};

}
}

#endif // PUFFIN_ASYNC_THREAD_EXECUTOR_HPP
