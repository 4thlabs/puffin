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

#ifndef PUFFIN_ASYNC_FROM_CALLBACK_HPP
#define PUFFIN_ASYNC_FROM_CALLBACK_HPP

#include <puffin/async/executor.hpp>

#include <atomic>
#include <coroutine>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace async {

/**
 * @brief Awaitable bridging a callback based operation to coroutines.
 *
 * On co_await, the coroutine is suspended and initiate(callback) is called. callback is a copyable
 * function object taking Args..., to call exactly once (from any thread, even inline from
 * initiate). The coroutine is then resumed on its executor (inline if it has none) and co_await
 * returns: nothing for no Args, the value for one, a std::tuple<Args...> otherwise.
 */
template<typename F, typename... Args>
class callback_awaitable {
public:
  using result_type = std::tuple<Args...>;

  /**
   * @brief The callback handed to the initiating function
   */
  class callback {
  public:
    explicit callback(callback_awaitable* self) noexcept
        : self_(self)
    {}

    template<typename... A>
    void operator()(A&&... args) const
    {
      self_->result_.emplace(std::forward<A>(args)...);

      if (self_->state_.exchange(completed, std::memory_order_acq_rel) == suspended)
        self_->resume();
    }

  private:
    callback_awaitable* self_;
  };

  explicit callback_awaitable(F initiate)
      : initiate_(std::move(initiate))
  {}

  // Only movable before being awaited, as returned from initiating functions
  callback_awaitable(callback_awaitable&& rhs) noexcept(std::is_nothrow_move_constructible_v<F>)
      : initiate_(std::move(rhs.initiate_))
  {}

  callback_awaitable(const callback_awaitable&) = delete;
  callback_awaitable& operator=(const callback_awaitable&) = delete;
  callback_awaitable& operator=(callback_awaitable&&) = delete;

  bool await_ready() const noexcept { return false; }

  template<typename P>
  bool await_suspend(std::coroutine_handle<P> h)
  {
    handle_ = h;
    executor_ = executor_of(h);

    std::invoke(std::move(initiate_), callback { this });

    // If the callback already ran, do not suspend
    return state_.exchange(suspended, std::memory_order_acq_rel) != completed;
  }

  auto await_resume()
  {
    if constexpr (sizeof...(Args) == 0)
      return;
    else if constexpr (sizeof...(Args) == 1)
      return std::move(std::get<0>(*result_));
    else
      return std::move(*result_);
  }

protected:
  result_type& result() noexcept { return *result_; }

private:
  static constexpr int pending = 0;
  static constexpr int suspended = 1;
  static constexpr int completed = 2;

  void resume()
  {
    // Copies, the awaitable may be destroyed as soon as the coroutine is resumed
    auto executor = executor_;
    auto handle = handle_;

    if (executor)
      executor.post(handle);
    else
      handle.resume();
  }

  F initiate_;
  std::optional<result_type> result_;
  std::coroutine_handle<> handle_;
  any_executor executor_;
  std::atomic<int> state_ { pending };
};

/**
 * @brief Makes a callback based operation awaitable, Args being the callback parameters:
 *
 *   auto [ec, n] = co_await from_callback<std::error_code, std::size_t>([&](auto cb) {
 *     socket.async_read_some(buffer, cb);
 *   });
 */
template<typename... Args, typename F>
callback_awaitable<std::decay_t<F>, Args...> from_callback(F&& initiate)
{
  return callback_awaitable<std::decay_t<F>, Args...>(std::forward<F>(initiate));
}

}
}

#endif // PUFFIN_ASYNC_FROM_CALLBACK_HPP
