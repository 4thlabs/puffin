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

#ifndef PUFFIN_REST_DETAIL_SERVICE_HPP
#define PUFFIN_REST_DETAIL_SERVICE_HPP

#include <puffin/async/async.hpp>
#include <puffin/async/try_await.hpp>
#include <puffin/rest/codec.hpp>
#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/webkit/http/response.hpp>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

// Server side: calling the service of an endpoint and writing its result or error

namespace puffin {
namespace rest {
namespace detail {

/// True if the service implements the endpoint, with or without the context as last argument
template<typename Service, typename R, typename Ctx, typename Values = typename R::values>
struct implements;

template<typename Service, typename R, typename Ctx, typename... Vs>
struct implements<Service, R, Ctx, typelist<Vs...>>
    : std::bool_constant<std::is_invocable_v<Service&, typename R::endpoint, Vs...> ||
                         std::is_invocable_v<Service&, typename R::endpoint, Vs..., Ctx&>> {};

/// Calls the service with the argument values, and the context when it takes it
template<typename R, typename Service, typename Ctx, typename Values>
decltype(auto) invoke_service(Service& service, Ctx& ctx, Values&& values)
{
  using E = typename R::endpoint;

  return std::apply(
      [&](auto&&... args) -> decltype(auto) {
        if constexpr (std::is_invocable_v<Service&, E, decltype(args)..., Ctx&>)
          return service(E {}, std::move(args)..., ctx);
        else
          return service(E {}, std::move(args)...);
      },
      std::forward<Values>(values));
}

template<typename T>
struct async_value {
  using type = T;
};

template<typename T>
struct async_value<async::async<T>> {
  using type = T;
};

/// The service call as a coroutine, whether the service is synchronous or not
template<typename R, typename Service, typename Ctx, typename Values>
async::async<typename R::result_type> call_service(Service& service, Ctx& ctx, Values values)
{
  using expected = typename R::result_type;
  using raw = decltype(invoke_service<R>(service, ctx, std::move(values)));
  using value_type = typename async_value<raw>::type;

  static_assert(!std::is_void_v<expected> || std::is_void_v<value_type>,
                "the endpoint declares no returns<> but the service returns a value");
  static_assert(std::is_void_v<expected> || std::is_convertible_v<value_type, expected>,
                "the service result does not match the returns<> of the endpoint");

  if constexpr (async::is_async_v<raw>)
    co_return co_await invoke_service<R>(service, ctx, std::move(values));
  else if constexpr (std::is_void_v<expected>)
    invoke_service<R>(service, ctx, std::move(values));
  else
    co_return invoke_service<R>(service, ctx, std::move(values));
}

/// Writes the success response of endpoint R
template<typename R, typename... Result>
void write_result(webkit::response& res, const Result&... result)
{
  using returns = typename R::returns;

  if constexpr (sizeof...(Result) > 0) {
    using codec = result_codec_t<R>;
    res.body(codec::encode(result...), std::string(codec::content_type));
  }

  res.status(returns::status);
}

inline std::string json_escape(std::string_view s)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;

  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      out += "\\u00";
      out += digits[c >> 4];
      out += digits[c & 0x0F];
    } else {
      out += static_cast<char>(c);
    }
  }

  return out;
}

/// The response of an http_error: its status and {"error": message}
inline void write_error(webkit::response& res, const http_error& e)
{
  res.status(e.status());
  res.body("{\"error\":\"" + json_escape(e.what()) + "\"}", "application/json");
}

/// Runs one layer of a call, an http_error it throws becomes the response so that the outer layers see it
template<typename Ctx>
async::async<void> answer_errors(Ctx& ctx, async::async<void> layer)
{
  auto outcome = co_await async::try_await(std::move(layer));

  if (!outcome)
    write_error(ctx.response(), http_error_of(outcome));
}

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_SERVICE_HPP
