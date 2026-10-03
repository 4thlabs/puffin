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

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

namespace puffin {
namespace async {

template<typename T = void>
class async;

namespace detail {

class promise_base {
public:
  /**
   * @brief Resumes the awaiting coroutine. Symmetric transfer when it runs on the same executor,
   *        otherwise it is posted back to its own executor.
   */
  struct final_awaitable {
    bool await_ready() const noexcept { return false; }

    template<typename P>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) const noexcept
    {
      promise_base& p = h.promise();

      if (!p.continuation_)
        return std::noop_coroutine();

      if (p.continuation_executor_ && !(p.continuation_executor_ == p.executor_)) {
        p.continuation_executor_.post(p.continuation_);
        return std::noop_coroutine();
      }

      return p.continuation_;
    }

    void await_resume() const noexcept {}
  };

  std::suspend_always initial_suspend() const noexcept { return {}; }
  final_awaitable final_suspend() const noexcept { return {}; }

  any_executor& executor() noexcept { return executor_; }

  void set_continuation(std::coroutine_handle<> continuation, any_executor executor) noexcept
  {
    continuation_ = continuation;
    continuation_executor_ = std::move(executor);
  }

private:
  any_executor executor_;
  std::coroutine_handle<> continuation_;
  any_executor continuation_executor_;
};

template<typename T>
class promise final : public promise_base {
public:
  async<T> get_return_object() noexcept;

  template<typename U = T>
    requires std::convertible_to<U&&, T>
  void return_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
  {
    result_.template emplace<1>(std::forward<U>(value));
  }

  void unhandled_exception() noexcept { result_.template emplace<2>(std::current_exception()); }

  T result()
  {
    if (result_.index() == 2)
      std::rethrow_exception(std::get<2>(result_));

    return std::move(std::get<1>(result_));
  }

private:
  std::variant<std::monostate, T, std::exception_ptr> result_;
};

template<>
class promise<void> final : public promise_base {
public:
  async<void> get_return_object() noexcept;

  void return_void() noexcept {}

  void unhandled_exception() noexcept { exception_ = std::current_exception(); }

  void result()
  {
    if (exception_)
      std::rethrow_exception(exception_);
  }

private:
  std::exception_ptr exception_;
};

}

/**
 * @brief A lazy coroutine task. The body starts when the task is awaited (or spawned), on the
 *        executor of the awaiting coroutine, and resumes it once done.
 */
template<typename T>
class [[nodiscard]] async {
public:
  static_assert(!std::is_reference_v<T>, "async<T&> is not supported, use a pointer or std::reference_wrapper");

  using value_type = T;
  using promise_type = detail::promise<T>;
  using handle_type = std::coroutine_handle<promise_type>;

  async() noexcept = default;

  explicit async(handle_type handle) noexcept
      : handle_(handle)
  {}

  async(async&& rhs) noexcept
      : handle_(std::exchange(rhs.handle_, nullptr))
  {}

  async& operator=(async&& rhs) noexcept
  {
    if (this != &rhs) {
      if (handle_)
        handle_.destroy();

      handle_ = std::exchange(rhs.handle_, nullptr);
    }

    return *this;
  }

  async(const async&) = delete;
  async& operator=(const async&) = delete;

  ~async()
  {
    if (handle_)
      handle_.destroy();
  }

  bool valid() const noexcept { return handle_ != nullptr; }

  bool await_ready() const noexcept { return false; }

  template<typename P>
  std::coroutine_handle<> await_suspend(std::coroutine_handle<P> caller) noexcept
  {
    auto& p = handle_.promise();
    auto executor = executor_of(caller);

    if (!p.executor())
      p.executor() = executor;

    p.set_continuation(caller, std::move(executor));
    return handle_;
  }

  T await_resume() { return handle_.promise().result(); }

  /**
   * @brief Low level access, for drivers like co_spawn or when_all
   */
  handle_type handle() const noexcept { return handle_; }

private:
  handle_type handle_ = nullptr;
};

namespace detail {

template<typename T>
async<T> promise<T>::get_return_object() noexcept
{
  return async<T> { std::coroutine_handle<promise<T>>::from_promise(*this) };
}

inline async<void> promise<void>::get_return_object() noexcept
{
  return async<void> { std::coroutine_handle<promise<void>>::from_promise(*this) };
}

}

template<typename T>
struct is_async : std::false_type {};

template<typename T>
struct is_async<async<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_async_v = is_async<std::remove_cvref_t<T>>::value;

}
}

#endif // PUFFIN_ASYNC_ASYNC_HPP
