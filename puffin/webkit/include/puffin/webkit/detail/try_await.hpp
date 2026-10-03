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

#ifndef PUFFIN_WEBKIT_DETAIL_TRY_AWAIT_HPP
#define PUFFIN_WEBKIT_DETAIL_TRY_AWAIT_HPP

#include <puffin/async.hpp>

#include <exception>
#include <optional>
#include <utility>

// TODO: Move to puffin::async once its API is settled

namespace puffin {
namespace webkit {
namespace detail {

/**
 * @brief The value of an awaited operation, or the exception it threw.
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

/// Awaits an operation and returns its outcome instead of throwing
template<typename T>
async::async<outcome<T>> try_await(async::async<T> operation)
{
  try {
    co_return outcome<T>{co_await std::move(operation), nullptr};
  } catch (...) {
    co_return outcome<T>{std::nullopt, std::current_exception()};
  }
}

} // namespace detail
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_DETAIL_TRY_AWAIT_HPP
