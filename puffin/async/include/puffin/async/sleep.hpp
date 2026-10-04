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

#ifndef PUFFIN_ASYNC_SLEEP_HPP
#define PUFFIN_ASYNC_SLEEP_HPP

#include <puffin/async/executor.hpp>
#include <puffin/async/impl/thread_executor.hpp>

#include <chrono>
#include <coroutine>

namespace puffin {
namespace async {

/**
 * @brief Suspends the awaiting coroutine for a duration, then resumes it on its executor:
 *
 *   co_await sleep_for(std::chrono::milliseconds(200));
 *
 * The executor needs a timer (TimedExecutor: thread_executor, inline_executor, the asio and Qt executors), or
 * std::logic_error is thrown. Without any executor (sync_wait with none), it sleeps like inline_executor: blocking.
 */
class sleep_for {
public:
  template<typename Rep, typename Period>
  explicit sleep_for(std::chrono::duration<Rep, Period> duration)
      : delay_(std::chrono::ceil<std::chrono::nanoseconds>(duration))
  {}

  bool await_ready() const noexcept { return delay_.count() <= 0; }

  template<typename P>
  void await_suspend(std::coroutine_handle<P> h) const
  {
    any_executor executor = executor_of(h);

    if (executor)
      executor.post_after(h, delay_);
    else
      inline_executor {}.post_after(h, delay_);
  }

  void await_resume() const noexcept {}

private:
  std::chrono::nanoseconds delay_;
};

}
}

#endif // PUFFIN_ASYNC_SLEEP_HPP
