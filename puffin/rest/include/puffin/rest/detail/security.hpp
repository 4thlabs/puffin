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

#ifndef PUFFIN_REST_DETAIL_SECURITY_HPP
#define PUFFIN_REST_DETAIL_SECURITY_HPP

#include <puffin/async/async.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/rest/params.hpp>
#include <puffin/webkit/http/request.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {

/// The value of an api_key<> or api_key_query<> scheme, given to the validators
struct api_key_value {
  std::string_view name; ///< Header or query parameter name
  std::string value;
};

namespace detail {

/// The value a validator receives for a security scheme
template<typename S>
struct scheme_value;

template<fixed_string N>
struct scheme_value<api_key<N>> {
  using type = api_key_value;
};

template<fixed_string N>
struct scheme_value<api_key_query<N>> {
  using type = api_key_value;
};

template<>
struct scheme_value<bearer_auth> {
  using type = bearer;
};

template<typename S>
using scheme_value_t = typename scheme_value<S>::type;

/// The credentials of scheme S sent with the request, nullopt if absent
template<typename S>
std::optional<scheme_value_t<S>> credentials(const webkit::request& req)
{
  if constexpr (std::is_same_v<S, bearer_auth>) {
    auto value = req.headers().get("Authorization");
    return value ? param_traits<bearer>::parse(*value) : std::nullopt;
  } else if constexpr (S::in_query) {
    auto query = req.query();
    auto it = query.find(S::name.view());
    return it == query.end() ? std::nullopt : std::optional(api_key_value {S::name.view(), it->second});
  } else {
    auto value = req.headers().get(S::name.view());
    return value ? std::optional(api_key_value {S::name.view(), std::string(*value)}) : std::nullopt;
  }
}

template<typename V, typename Value, typename Ctx>
inline constexpr bool validates_v = std::is_invocable_v<V&, Value, Ctx&> || std::is_invocable_v<V&, Value>;

/// Index of the first validator of the tuple accepting Value, the tuple size if none does
template<typename Validators, typename Value, typename Ctx>
constexpr std::size_t validator_index()
{
  return []<std::size_t... Is>(std::index_sequence<Is...>) {
    return first_true(std::array<bool, sizeof...(Is) + 1> {
        validates_v<std::tuple_element_t<Is, Validators>, Value, Ctx>..., false});
  }(std::make_index_sequence<std::tuple_size_v<Validators>>());
}

template<typename T>
async::async<bool> to_async_bool(T value)
{
  if constexpr (async::is_async_v<T>)
    co_return static_cast<bool>(co_await std::move(value));
  else
    co_return static_cast<bool>(value);
}

/// Runs a validator, with the context when it takes it. Validators return bool or async<bool>.
template<typename V, typename Value, typename Ctx>
async::async<bool> run_validator(V& validator, Value value, Ctx& ctx)
{
  if constexpr (std::is_invocable_v<V&, Value, Ctx&>)
    return to_async_bool(validator(std::move(value), ctx));
  else
    return to_async_bool(validator(std::move(value)));
}

/// Checks the credentials of scheme S with the first validator accepting them, 401 when missing or rejected
template<typename S, typename Validators, typename Ctx>
async::async<void> authorize_scheme(Validators& validators, Ctx& ctx)
{
  using value_type = scheme_value_t<S>;
  constexpr std::size_t index = validator_index<Validators, value_type, Ctx>();

  static_assert(index < std::tuple_size_v<Validators>,
                "no validator for a security scheme of the api: pass rest::validators(...) to mount, "
                "with a callable taking api_key_value or bearer (and optionally the context)");

  std::optional<value_type> value = credentials<S>(ctx.request());

  if (!value)
    throw http_error(webkit::status::unauthorized, "missing credentials");

  if (!co_await run_validator(std::get<index>(validators), std::move(*value), ctx))
    throw http_error(webkit::status::unauthorized, "invalid credentials");
}

/// Checks every security scheme of endpoint R
template<typename R, std::size_t I = 0, typename Validators, typename Ctx>
async::async<void> authorize(Validators& validators, Ctx& ctx)
{
  if constexpr (I < R::schemes::size) {
    co_await authorize_scheme<at_t<I, typename R::schemes>>(validators, ctx);
    co_await authorize<R, I + 1>(validators, ctx);
  }
}

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_SECURITY_HPP
