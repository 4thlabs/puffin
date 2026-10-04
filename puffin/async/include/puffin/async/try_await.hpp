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

#ifndef PUFFIN_ASYNC_TRY_AWAIT_HPP
#define PUFFIN_ASYNC_TRY_AWAIT_HPP

#include <puffin/async/async.hpp>

#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace puffin {
namespace async {

/**
 * @brief The value of an awaited task, or the exception it threw.
 *
 * co_await is not allowed in a catch block: a coroutine that must await something on failure (retry, cleanup)
 * captures the outcome with try_await() and branches on it outside of the handler.
 */
template<typename T>
struct outcome {
  std::optional<T> value;
  std::exception_ptr error; ///< Set when there is no value

  explicit operator bool() const noexcept { return value.has_value(); }

  [[noreturn]] void rethrow() const { std::rethrow_exception(error); }
};

/**
 * @brief Awaits a task and returns its outcome instead of throwing. void results become std::monostate.
 *
 *   auto first = co_await try_await(fetch());
 *   if (!first)
 *     co_await reconnect();
 */
template<typename T>
async<outcome<std::conditional_t<std::is_void_v<T>, std::monostate, T>>> try_await(async<T> task)
{
  using value_type = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

  try {
    if constexpr (std::is_void_v<T>) {
      co_await std::move(task);
      co_return outcome<value_type>{value_type{}, nullptr};
    } else {
      co_return outcome<value_type>{co_await std::move(task), nullptr};
    }
  } catch (...) {
    co_return outcome<value_type>{std::nullopt, std::current_exception()};
  }
}

} // namespace async
} // namespace puffin

#endif // PUFFIN_ASYNC_TRY_AWAIT_HPP
