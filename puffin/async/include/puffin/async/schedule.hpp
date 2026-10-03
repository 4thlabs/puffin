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

#ifndef PUFFIN_ASYNC_SCHEDULE_HPP
#define PUFFIN_ASYNC_SCHEDULE_HPP

#include <puffin/async/executor.hpp>

#include <coroutine>

namespace puffin {
namespace async {

/**
 * @brief Moves the awaiting coroutine to the given executor: co_await schedule_on(ex);
 *        Following awaits in this coroutine (and the tasks it awaits) run on ex.
 */
class schedule_on {
public:
  explicit schedule_on(any_executor executor) noexcept
      : executor_(std::move(executor))
  {}

  bool await_ready() const noexcept { return !executor_; }

  template<typename P>
  void await_suspend(std::coroutine_handle<P> h) const
  {
    if constexpr (ExecutorAwarePromise<P>)
      h.promise().executor() = executor_;

    executor_.post(h);
  }

  void await_resume() const noexcept {}

private:
  any_executor executor_;
};

/**
 * @brief Gives the executor of the current coroutine: any_executor ex = co_await this_executor;
 */
struct this_executor_t {
  struct awaiter {
    bool await_ready() const noexcept { return false; }

    template<typename P>
    bool await_suspend(std::coroutine_handle<P> h) noexcept
    {
      executor = executor_of(h);
      return false;
    }

    any_executor await_resume() noexcept { return std::move(executor); }

    any_executor executor;
  };

  awaiter operator co_await() const noexcept { return {}; }
};

inline constexpr this_executor_t this_executor {};

}
}

#endif // PUFFIN_ASYNC_SCHEDULE_HPP
