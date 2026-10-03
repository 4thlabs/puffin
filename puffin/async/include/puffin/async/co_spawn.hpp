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

#ifndef PUFFIN_ASYNC_CO_SPAWN_HPP
#define PUFFIN_ASYNC_CO_SPAWN_HPP

#include <puffin/async/async.hpp>
#include <puffin/async/executor.hpp>

#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace puffin {
namespace async {

namespace detail {

/**
 * @brief Fire and forget coroutine, owning its own frame. Used to drive tasks from non
 *        coroutine code.
 */
class detached_task {
public:
  struct promise_type {
    detached_task get_return_object() noexcept
    {
      return detached_task { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_never final_suspend() const noexcept { return {}; }

    void return_void() noexcept {}
    void unhandled_exception() noexcept { std::terminate(); }

    any_executor& executor() noexcept { return executor_; }

    any_executor executor_;
  };

  explicit detached_task(std::coroutine_handle<promise_type> handle) noexcept
      : handle_(handle)
  {}

  /**
   * @brief Starts the coroutine on the executor, or inline when the executor is empty
   */
  void start(any_executor executor) &&
  {
    auto handle = std::exchange(handle_, nullptr);
    handle.promise().executor_ = executor;

    if (executor)
      executor.post(handle);
    else
      handle.resume();
  }

private:
  std::coroutine_handle<promise_type> handle_;
};

template<typename T, typename C>
void invoke_completion(C& completion, std::exception_ptr ex, T* value)
{
  if constexpr (!std::is_void_v<T> && std::invocable<C&, std::exception_ptr, T>) {
    static_assert(std::is_default_constructible_v<T>,
                  "completion(std::exception_ptr, T) requires T to be default constructible");

    if (value)
      std::invoke(completion, ex, std::move(*value));
    else
      std::invoke(completion, ex, T {});
  } else {
    static_assert(std::invocable<C&, std::exception_ptr>,
                  "completion must be invocable with (std::exception_ptr) or (std::exception_ptr, T)");
    std::invoke(completion, ex);
  }
}

template<typename F, typename C>
detached_task spawn_entry(F factory, C completion)
{
  using task_type = std::invoke_result_t<F&>;
  using value_type = typename task_type::value_type;

  std::exception_ptr ex = nullptr;

  if constexpr (std::is_void_v<value_type>) {
    try {
      co_await std::invoke(factory);
    } catch (...) {
      ex = std::current_exception();
    }

    detail::invoke_completion<void>(completion, ex, nullptr);
  } else {
    std::optional<value_type> value;

    try {
      value.emplace(co_await std::invoke(factory));
    } catch (...) {
      ex = std::current_exception();
    }

    detail::invoke_completion<value_type>(completion, ex, value ? &*value : nullptr);
  }
}

}

/**
 * @brief Default completion of co_spawn, discards the result and any exception
 */
struct detached_t {
  void operator()(std::exception_ptr) const noexcept {}
};

inline constexpr detached_t detached {};

/**
 * @brief Starts a task on an executor without waiting for it.
 *
 * The completion is called on the executor once the task is done, with (std::exception_ptr) or
 * (std::exception_ptr, T). The task is started from a call posted to the executor (inline if the
 * executor is empty).
 */
template<typename T, typename C = detached_t>
void co_spawn(any_executor executor, async<T> task, C completion = {})
{
  detail::spawn_entry([task = std::move(task)]() mutable { return std::move(task); }, std::move(completion))
      .start(std::move(executor));
}

/**
 * @brief Same as above, but the task is created by calling factory, which is kept alive (with its
 *        captures) until the task is done. Prefer this form with capturing lambdas.
 */
template<typename F, typename C = detached_t>
  requires std::invocable<F&> && is_async_v<std::invoke_result_t<F&>>
void co_spawn(any_executor executor, F factory, C completion = {})
{
  detail::spawn_entry(std::move(factory), std::move(completion)).start(std::move(executor));
}

}
}

#endif // PUFFIN_ASYNC_CO_SPAWN_HPP
