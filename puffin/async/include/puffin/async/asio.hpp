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

#ifndef PUFFIN_ASYNC_ASIO_HPP
#define PUFFIN_ASYNC_ASIO_HPP

// asio adapter for puffin::async. Uses standalone asio by default, define
// PUFFIN_ASYNC_USE_BOOST_ASIO to use Boost.Asio instead.

#include <puffin/async/async.hpp>
#include <puffin/async/from_callback.hpp>

#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/system_error.hpp>
#else
#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/system_error.hpp>
#endif

#include <chrono>
#include <coroutine>
#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace async {

namespace detail {

#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
namespace net = ::boost::asio;
using net_error_code = ::boost::system::error_code;
using net_system_error = ::boost::system::system_error;
#else
namespace net = ::asio;
using net_error_code = ::asio::error_code;
using net_system_error = ::asio::system_error;
#endif

}

namespace asio {

/**
 * @brief Executor resuming coroutines on an asio executor (typically an io_context). Lightweight
 *        handle, pass it by value: co_spawn(asio::executor { io_context }, task());
 */
class executor {
public:
  using inner_executor_type = ::puffin::async::detail::net::any_io_executor;

  executor(inner_executor_type inner)
      : inner_(std::move(inner))
  {}

  executor(::puffin::async::detail::net::io_context& context)
      : inner_(context.get_executor())
  {}

  void post(std::coroutine_handle<> h) const
  {
    ::puffin::async::detail::net::post(inner_, [h] { h.resume(); });
  }

  const inner_executor_type& get_inner_executor() const noexcept { return inner_; }

  friend bool operator==(const executor& lhs, const executor& rhs) noexcept { return lhs.inner_ == rhs.inner_; }

private:
  inner_executor_type inner_;
};

/**
 * @brief Completion token making asio operations awaitable from puffin::async coroutines.
 *
 * With use_async, a leading error_code (or exception_ptr) is thrown when set, and co_await returns
 * the remaining arguments (nothing, the value, or a tuple):
 *
 *   std::size_t n = co_await socket.async_read_some(buffer, asio::use_async);
 *
 * With use_async_tuple, nothing is thrown and co_await returns all the arguments as a tuple:
 *
 *   auto [ec, n] = co_await socket.async_read_some(buffer, asio::use_async_tuple);
 *
 * The operation must not outlive the awaiting coroutine frame (keep sockets and timers in it).
 */
template<bool Throw>
struct basic_use_async_t {
  constexpr basic_use_async_t() noexcept = default;
};

using use_async_t = basic_use_async_t<true>;
using use_async_tuple_t = basic_use_async_t<false>;

inline constexpr use_async_t use_async {};
inline constexpr use_async_tuple_t use_async_tuple {};

namespace detail {

template<bool Throw, typename F, typename... Args>
class op_awaitable : public callback_awaitable<F, Args...> {
public:
  using callback_awaitable<F, Args...>::callback_awaitable;

  auto await_resume()
  {
    auto& result = this->result();

    if constexpr (!Throw) {
      return std::move(result);
    } else if constexpr (sizeof...(Args) == 0) {
      return;
    } else {
      using first_type = std::tuple_element_t<0, std::tuple<Args...>>;

      if constexpr (std::is_same_v<first_type, ::puffin::async::detail::net_error_code>) {
        if (std::get<0>(result))
          throw ::puffin::async::detail::net_system_error(std::get<0>(result));

        return drop_first(std::move(result));
      } else if constexpr (std::is_same_v<first_type, std::exception_ptr>) {
        if (std::get<0>(result))
          std::rethrow_exception(std::get<0>(result));

        return drop_first(std::move(result));
      } else if constexpr (sizeof...(Args) == 1) {
        return std::move(std::get<0>(result));
      } else {
        return std::move(result);
      }
    }
  }

private:
  template<typename First, typename... Rest>
  static auto drop_first(std::tuple<First, Rest...>&& t)
  {
    if constexpr (sizeof...(Rest) == 0)
      return;
    else if constexpr (sizeof...(Rest) == 1)
      return std::move(std::get<1>(t));
    else
      return std::apply([](First&&, Rest&&... rest) { return std::tuple<Rest...>(std::move(rest)...); },
                        std::move(t));
  }
};

}

}

}
}

#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
namespace boost {
#endif
namespace asio {

template<bool Throw, typename R, typename... Args>
class async_result<::puffin::async::asio::basic_use_async_t<Throw>, R(Args...)> {
public:
  template<typename Initiation, typename... InitArgs>
  static auto initiate(Initiation&& initiation, ::puffin::async::asio::basic_use_async_t<Throw>, InitArgs&&... args)
  {
    auto init = [initiation = std::forward<Initiation>(initiation),
                 ... args = std::forward<InitArgs>(args)](auto callback) mutable {
      std::move(initiation)(std::move(callback), std::move(args)...);
    };

    return ::puffin::async::asio::detail::op_awaitable<Throw, decltype(init), std::decay_t<Args>...>(std::move(init));
  }
};

}
#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
}
#endif

namespace puffin {
namespace async {
namespace asio {

/**
 * @brief Suspends the coroutine for the given duration, using a timer on the given executor
 */
template<typename Rep, typename Period>
async<void> sleep_for(executor ex, std::chrono::duration<Rep, Period> duration)
{
  ::puffin::async::detail::net::steady_timer timer(ex.get_inner_executor(), duration);
  co_await timer.async_wait(use_async);
}

}
}
}

#endif // PUFFIN_ASYNC_ASIO_HPP
