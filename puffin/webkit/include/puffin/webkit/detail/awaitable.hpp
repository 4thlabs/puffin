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

#ifndef PUFFIN_WEBKIT_DETAIL_AWAITABLE_HPP
#define PUFFIN_WEBKIT_DETAIL_AWAITABLE_HPP

#include <concepts>
#include <coroutine>
#include <utility>

// TODO: Move to puffin::async once its API is settled

namespace puffin {
namespace webkit {
namespace detail {

template<typename T>
concept awaiter = requires(T& a, std::coroutine_handle<> h) {
  { a.await_ready() } -> std::convertible_to<bool>;
  a.await_suspend(h);
  a.await_resume();
};

template<typename T>
decltype(auto) get_awaiter(T&& t)
{
  if constexpr (requires { std::forward<T>(t).operator co_await(); })
    return std::forward<T>(t).operator co_await();
  else if constexpr (requires { operator co_await(std::forward<T>(t)); })
    return operator co_await(std::forward<T>(t));
  else
    return std::forward<T>(t);
}

template<typename T>
using awaiter_t = decltype(get_awaiter(std::declval<T>()));

} // namespace detail

/**
 * @brief Anything usable with co_await (an awaiter, or a type with operator co_await)
 */
template<typename T>
concept Awaitable = detail::awaiter<std::remove_reference_t<detail::awaiter_t<T>>>;

/// The type produced by co_await on an awaitable
template<Awaitable T>
using await_result_t = decltype(std::declval<detail::awaiter_t<T>&>().await_resume());

/**
 * @brief An awaitable whose co_await produces a value convertible to R
 */
template<typename T, typename R>
concept AwaitableOf = Awaitable<T> && std::convertible_to<await_result_t<T>, R>;

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_DETAIL_AWAITABLE_HPP
