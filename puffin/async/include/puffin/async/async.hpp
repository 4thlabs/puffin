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

#ifndef PUFFIN_ASYNC_ASYNC_HPP
#define PUFFIN_ASYNC_ASYNC_HPP

#include <puffin/async/executor.hpp>
#include <puffin/async/impl/thread_executor.hpp>

#include <coroutine>
#include <exception>

namespace puffin {
namespace async {

struct co_entry_point {};

template<Executor E>
struct promise_base {
public:
  using executor_type = E;

  executor_type* executor = nullptr;
  std::coroutine_handle<> precursor = nullptr;
  std::exception_ptr exception = nullptr;

  promise_base() {}
};

/**
 * @brief The final_awaitable class
 */
template<typename P>
struct final_awaitable {
  bool await_ready() const noexcept { return false; }
  void await_resume() const noexcept {}

  void await_suspend(std::coroutine_handle<P> h) const noexcept {
    auto precursor = h.promise().precursor;
    auto executor = h.promise().executor;

    if (precursor)
      executor->post(precursor);
  }
};

template<typename T = void, Executor E = thread_executor>
class async  {
public:
  struct promise_type : public promise_base<E> {
    T value;

    promise_type()
        : promise_base<E>()
    {};

    async get_return_object()
    {
      return async { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    std::suspend_always initial_suspend() { return {}; }

    auto final_suspend() noexcept {
       return final_awaitable<promise_type>{};
    }

    void return_value(T v) noexcept
    {
      value = v;
    }

    void unhandled_exception()
    {
      this->exception = std::current_exception();
    }
  };

  explicit async(const std::coroutine_handle<promise_type> handle) noexcept
      : handle_(handle)
  {}

  async(async&& rhs) noexcept
      : handle_(rhs.handle_)
  {
    rhs.handle_ = nullptr;
  }

  ~async()
  {
    if (handle_)
      handle_.destroy();
  }

  template<typename Promise>
  void await_suspend(std::coroutine_handle<Promise> h) const noexcept
  {
    auto executor = h.promise().executor;
    handle_.promise().executor = executor;
    handle_.promise().precursor = h;

    executor->post(handle_);
  }

  bool await_ready() { return handle_.done(); }

  T await_resume() {
    if (handle_.promise().exception)
      std::rethrow_exception(handle_.promise().exception);

    return handle_.promise().value;
  }

private:
  std::coroutine_handle<promise_type> handle_;

  async(const async&) = delete;
  async& operator=(const async&) = delete;
};

template<Executor E>
class async<void, E> {
public:
  struct promise_type : public promise_base<E> {
    promise_type()
        : promise_base<E>()
    {}

    async get_return_object() {
      return async { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    std::suspend_always initial_suspend() { return {}; }

    auto final_suspend() noexcept {
      return final_awaitable<promise_type>{};
    }

    void return_void() {}

    void unhandled_exception() {
      this->exception = std::current_exception();
    }
  };

  explicit async(const std::coroutine_handle<promise_type> handle) noexcept
      : handle_(handle)
  {}

  async(async&& rhs) noexcept
      : handle_(rhs.handle_)
  {
    rhs.handle_ = nullptr;
  }

  ~async()
  {
    if (handle_)
      handle_.destroy();
  }

  template<typename Promise>
  void await_suspend(std::coroutine_handle<Promise> h) const noexcept
  {
    auto executor = h.promise().executor;
    handle_.promise().executor = executor;
    handle_.promise().precursor = h;

    executor->post(handle_);
  }

  bool await_ready() { return handle_.done(); }

  void await_resume() {
    if (handle_.promise().exception)
      std::rethrow_exception(handle_.promise().exception);
  }

private:
  std::coroutine_handle<promise_type> handle_;

  async(const async&) = delete;
  async& operator=(const async&) = delete;
};

template<Executor E>
class async<co_entry_point, E> {
public:
  struct promise_type : public promise_base<E> {
    promise_type()
        : promise_base<E>()
    {}

    async get_return_object() {
      return async { std::coroutine_handle<promise_type>::from_promise(*this) };
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_void() {}
    void unhandled_exception() {}
  };

  bool await_ready() { return true; }
  void await_suspend(std::coroutine_handle<promise_type> h) const noexcept {}
  void await_resume() {}

  explicit async(const std::coroutine_handle<promise_type> handle) noexcept
  {}
};

}
}

#include <puffin/async/co_spawn.hpp>

#endif // PUFFIN_ASYNC_ASYNC_HPP
