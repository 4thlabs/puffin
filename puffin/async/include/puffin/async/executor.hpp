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

#ifndef PUFFIN_ASYNC_EXECUTOR_HPP
#define PUFFIN_ASYNC_EXECUTOR_HPP

#include <concepts>
#include <coroutine>
#include <memory>
#include <type_traits>

namespace puffin {
namespace async {

/**
 * @brief An executor is anything able to resume a coroutine handle later, on its own context
 *        (a thread, an event loop, ...). post() must be thread safe.
 */
template<typename T>
concept Executor = requires(T& e, std::coroutine_handle<> h) {
  { e.post(h) };
};

/**
 * @brief The any_executor is a type erasure for executors
 *
 * Constructed from an lvalue, it references the executor, which must outlive it.
 * Constructed from an rvalue, it owns a copy of the executor (useful for lightweight
 * executor handles like the asio or Qt adapters).
 */
class any_executor {
public:
  any_executor() noexcept = default;

  template<typename E>
    requires(!std::same_as<std::remove_cvref_t<E>, any_executor> && Executor<std::remove_reference_t<E>>)
  any_executor(E&& e)
  {
    using executor_type = std::remove_reference_t<E>;

    if constexpr (std::is_lvalue_reference_v<E>) {
      object_ = const_cast<void*>(static_cast<const void*>(std::addressof(e)));
    } else {
      auto owned = std::make_shared<executor_type>(std::move(e));
      object_ = owned.get();
      owner_ = std::move(owned);
    }

    post_ = &post_impl<executor_type>;

    if constexpr (std::equality_comparable<executor_type>)
      equal_ = &equal_impl<executor_type>;
  }

  /**
   * @brief Schedule the resumption of h on the underlying executor
   */
  void post(std::coroutine_handle<> h) const { post_(object_, h); }

  explicit operator bool() const noexcept { return object_ != nullptr; }

  friend bool operator==(const any_executor& lhs, const any_executor& rhs) noexcept
  {
    if (lhs.object_ == rhs.object_)
      return true;

    if (lhs.post_ != rhs.post_ || !lhs.equal_)
      return false;

    return lhs.equal_(lhs.object_, rhs.object_);
  }

private:
  template<typename E>
  static void post_impl(void* object, std::coroutine_handle<> h)
  {
    static_cast<E*>(object)->post(h);
  }

  template<typename E>
  static bool equal_impl(const void* lhs, const void* rhs)
  {
    return *static_cast<const E*>(lhs) == *static_cast<const E*>(rhs);
  }

  std::shared_ptr<void> owner_;
  void* object_ = nullptr;
  void (*post_)(void*, std::coroutine_handle<>) = nullptr;
  bool (*equal_)(const void*, const void*) = nullptr;
};

/**
 * @brief A promise is executor aware when it exposes the executor its coroutine runs on
 */
template<typename P>
concept ExecutorAwarePromise = requires(P& p) {
  { p.executor() } -> std::same_as<any_executor&>;
};

/**
 * @brief Returns the executor of the coroutine behind h, or an empty executor if its promise
 *        is not executor aware. Meant for custom awaitables.
 */
template<typename P>
any_executor executor_of(std::coroutine_handle<P> h) noexcept
{
  if constexpr (ExecutorAwarePromise<P>)
    return h.promise().executor();
  else
    return {};
}

}
}

#endif // PUFFIN_ASYNC_EXECUTOR_HPP
