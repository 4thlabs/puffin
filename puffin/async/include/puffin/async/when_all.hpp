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

#ifndef PUFFIN_ASYNC_WHEN_ALL_HPP
#define PUFFIN_ASYNC_WHEN_ALL_HPP

#include <puffin/async/async.hpp>
#include <puffin/async/executor.hpp>

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace puffin {
namespace async {

/**
 * @brief void results are mapped to std::monostate in when_all results
 */
template<typename T>
using non_void_t = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

namespace detail {

struct when_all_counter {
  explicit when_all_counter(std::size_t count) noexcept
      : count(count + 1)
  {}

  /**
   * @brief Returns true when the caller was the last one to arrive
   */
  bool arrive() noexcept { return count.fetch_sub(1, std::memory_order_acq_rel) == 1; }

  std::atomic<std::size_t> count;
  std::coroutine_handle<> continuation;
  any_executor executor;
};

template<typename T>
struct when_all_slot {
  std::optional<non_void_t<T>> value;
  std::exception_ptr exception;
};

class when_all_child {
public:
  struct promise_type {
    struct final_awaitable {
      bool await_ready() const noexcept { return false; }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) const noexcept
      {
        auto& p = h.promise();
        auto& counter = *p.counter;

        if (!counter.arrive())
          return std::noop_coroutine();

        if (counter.executor && !(counter.executor == p.executor_)) {
          // Copies: once posted, the awaiter may resume and destroy the counter while post() runs
          auto executor = counter.executor;
          auto continuation = counter.continuation;
          executor.post(continuation);
          return std::noop_coroutine();
        }

        return counter.continuation;
      }

      void await_resume() const noexcept {}
    };

    when_all_child get_return_object() noexcept
    {
      return when_all_child { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    std::suspend_always initial_suspend() const noexcept { return {}; }
    final_awaitable final_suspend() const noexcept { return {}; }

    void return_void() noexcept {}
    void unhandled_exception() noexcept { std::terminate(); }

    any_executor& executor() noexcept { return executor_; }

    any_executor executor_;
    when_all_counter* counter = nullptr;
  };

  explicit when_all_child(std::coroutine_handle<promise_type> handle) noexcept
      : handle_(handle)
  {}

  when_all_child(when_all_child&& rhs) noexcept
      : handle_(std::exchange(rhs.handle_, nullptr))
  {}

  ~when_all_child()
  {
    if (handle_)
      handle_.destroy();
  }

  void start(when_all_counter& counter) noexcept
  {
    handle_.promise().counter = &counter;
    handle_.promise().executor_ = counter.executor;
    handle_.resume();
  }

private:
  std::coroutine_handle<promise_type> handle_;
};

template<typename T>
when_all_child make_when_all_child(async<T> task, when_all_slot<T>& slot)
{
  try {
    if constexpr (std::is_void_v<T>) {
      co_await std::move(task);
      slot.value.emplace();
    } else {
      slot.value.emplace(co_await std::move(task));
    }
  } catch (...) {
    slot.exception = std::current_exception();
  }
}

template<typename... Ts>
class when_all_awaitable {
public:
  explicit when_all_awaitable(async<Ts>&&... tasks)
      : tasks_(std::move(tasks)...)
      , counter_(sizeof...(Ts))
  {}

  bool await_ready() const noexcept { return false; }

  template<typename P>
  bool await_suspend(std::coroutine_handle<P> h)
  {
    counter_.continuation = h;
    counter_.executor = executor_of(h);

    start(std::index_sequence_for<Ts...> {});

    return !counter_.arrive();
  }

  std::tuple<non_void_t<Ts>...> await_resume()
  {
    std::apply(
        [](auto&... slot) {
          (
              [&] {
                if (slot.exception)
                  std::rethrow_exception(slot.exception);
              }(),
              ...);
        },
        slots_);

    return std::apply([](auto&... slot) { return std::tuple<non_void_t<Ts>...>(std::move(*slot.value)...); },
                      slots_);
  }

private:
  template<std::size_t... I>
  void start(std::index_sequence<I...>)
  {
    children_.reserve(sizeof...(Ts));
    (children_.push_back(make_when_all_child(std::move(std::get<I>(tasks_)), std::get<I>(slots_))), ...);

    for (auto& child : children_)
      child.start(counter_);
  }

  std::tuple<async<Ts>...> tasks_;
  std::tuple<when_all_slot<Ts>...> slots_;
  std::vector<when_all_child> children_;
  when_all_counter counter_;
};

template<typename T>
class when_all_range_awaitable {
public:
  explicit when_all_range_awaitable(std::vector<async<T>>&& tasks)
      : tasks_(std::move(tasks))
      , slots_(tasks_.size())
      , counter_(tasks_.size())
  {}

  bool await_ready() const noexcept { return tasks_.empty(); }

  template<typename P>
  bool await_suspend(std::coroutine_handle<P> h)
  {
    counter_.continuation = h;
    counter_.executor = executor_of(h);

    children_.reserve(tasks_.size());
    for (std::size_t i = 0; i < tasks_.size(); ++i)
      children_.push_back(make_when_all_child(std::move(tasks_[i]), slots_[i]));

    for (auto& child : children_)
      child.start(counter_);

    return !counter_.arrive();
  }

  std::vector<non_void_t<T>> await_resume()
  {
    for (auto& slot : slots_)
      if (slot.exception)
        std::rethrow_exception(slot.exception);

    std::vector<non_void_t<T>> results;
    results.reserve(slots_.size());

    for (auto& slot : slots_)
      results.push_back(std::move(*slot.value));

    return results;
  }

private:
  std::vector<async<T>> tasks_;
  std::vector<when_all_slot<T>> slots_;
  std::vector<when_all_child> children_;
  when_all_counter counter_;
};

}

/**
 * @brief Runs all tasks concurrently and returns their results once they are all done. Tasks are
 *        started in order on the awaiting coroutine's executor. If some of them fail, the first
 *        exception (in argument order) is rethrown once all of them are done.
 */
template<typename... Ts>
async<std::tuple<non_void_t<Ts>...>> when_all(async<Ts>... tasks)
{
  co_return co_await detail::when_all_awaitable<Ts...>(std::move(tasks)...);
}

template<typename T>
async<std::vector<non_void_t<T>>> when_all(std::vector<async<T>> tasks)
{
  co_return co_await detail::when_all_range_awaitable<T>(std::move(tasks));
}

}
}

#endif // PUFFIN_ASYNC_WHEN_ALL_HPP
