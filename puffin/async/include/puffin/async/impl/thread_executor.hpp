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

#include <thread>
#include <condition_variable>
#include <coroutine>
#include <stack>

namespace puffin {
namespace async {

/**
 * @brief A very basy single threaded executor
 */
class thread_executor final {
public:
  struct awaitable {
    awaitable(thread_executor* e)
        : executor(e)
    {}

    bool await_ready() { return false; }

    template<typename P>
    void await_suspend(std::coroutine_handle<P> h) const noexcept {
      h.promise().executor = executor;
      executor->post(h);
    }

    void await_resume() {}

    thread_executor* executor;
  };

  thread_executor()
      :running_(false)
  {}

  ~thread_executor()
  {
    stop();

    if(thread_.joinable())
      thread_.join();
  }

  void join() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable())
      thread_.join();
  }

  void post(std::coroutine_handle<> h)
  {
    std::lock_guard<std::mutex> lock(stack_mutex_);
    frames_.push(h);
    cv_.notify_all();
  }

  awaitable schedule() {
    return awaitable {this};
  }

  void run(bool blocking = true) {
    running_ = true;
    thread_ = std::thread(&thread_executor::pump, this);

    if (blocking)
      thread_.join();
  }

  void stop() {
    running_ = false;
    cv_.notify_all();
  }

private:

  std::coroutine_handle<> pop() {
    std::lock_guard<std::mutex> lock(stack_mutex_);
    auto res = frames_.top();
    frames_.pop();
    return res;
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(stack_mutex_);
    return frames_.empty();
  }

  void pump()
  {
    using namespace std::chrono_literals;

    while(running_) {
      std::unique_lock<std::mutex> lk(cv_mutex_);
      cv_.wait(lk, [&]() { return !empty(); });

      auto coro = pop();

      if (!coro.done())
        coro.resume();
    }
  }

private:
  std::condition_variable cv_;

  std::mutex stack_mutex_;
  std::mutex cv_mutex_;


  std::atomic<bool> running_;

  std::stack<std::coroutine_handle<>> frames_;
  std::thread thread_;
};

}
}

#endif // PUFFIN_ASYNC_THREAD_EXECUTOR_HPP
