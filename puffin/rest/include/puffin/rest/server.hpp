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

#ifndef PUFFIN_REST_SERVER_HPP
#define PUFFIN_REST_SERVER_HPP

#include <puffin/async.hpp>
#include <puffin/rest/codec.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/webkit/server/server.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
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

/**
 * @brief Security validators of a mount: callables taking the scheme value (api_key_value, bearer) and
 *        optionally the context, returning bool or async<bool>. False answers 401.
 *
 * Every security scheme used by the mounted api needs a validator, checked at compile time.
 */
template<typename... Validators>
struct validator_set {
  std::tuple<Validators...> items;
};

template<typename... Validators>
validator_set<std::decay_t<Validators>...> validators(Validators&&... v)
{
  return {{std::forward<Validators>(v)...}};
}

/**
 * @brief Server interceptors of a mount, run around each call from the first to the last:
 *
 *   struct audit {
 *     template<typename Info>
 *     async<void> operator()(Info info, auto& ctx, rest::next_handler next)
 *     {
 *       co_await next();
 *       log(Info::method_name, Info::path_template, ctx.response().status_code());
 *     }
 *   };
 *
 * An interceptor may answer by itself (set the response or throw http_error) without calling next().
 */
template<typename... Interceptors>
struct interceptor_set {
  std::tuple<Interceptors...> items;
};

template<typename... Interceptors>
interceptor_set<std::decay_t<Interceptors>...> interceptors(Interceptors&&... i)
{
  return {{std::forward<Interceptors>(i)...}};
}

/// Continues a server call: the next interceptor, or security, decoding and the service
class next_handler {
public:
  explicit next_handler(std::function<async::async<void>()> next)
      : next_(std::move(next))
  {}

  async::async<void> operator()() const { return next_(); }

private:
  std::function<async::async<void>()> next_;
};

namespace detail {

template<typename T>
struct is_validator_set : std::false_type {};

template<typename... V>
struct is_validator_set<validator_set<V...>> : std::true_type {};

template<typename T>
struct is_interceptor_set : std::false_type {};

template<typename... I>
struct is_interceptor_set<interceptor_set<I...>> : std::true_type {};

template<typename T>
inline constexpr bool is_service_v = !is_validator_set<T>::value && !is_interceptor_set<T>::value;

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

/// True if the service implements the endpoint, with or without the context as last argument
template<typename Service, typename R, typename Ctx, typename Values = typename R::values>
struct implements;

template<typename Service, typename R, typename Ctx, typename... Vs>
struct implements<Service, R, Ctx, typelist<Vs...>>
    : std::bool_constant<std::is_invocable_v<Service&, typename R::endpoint, Vs...> ||
                         std::is_invocable_v<Service&, typename R::endpoint, Vs..., Ctx&>> {};

template<typename V, typename Value, typename Ctx>
inline constexpr bool validates_v = std::is_invocable_v<V&, Value, Ctx&> || std::is_invocable_v<V&, Value>;

template<std::size_t N>
constexpr std::size_t first_true(const std::array<bool, N>& a)
{
  for (std::size_t i = 0; i < N; ++i) {
    if (a[i])
      return i;
  }

  return N;
}

template<std::size_t N>
constexpr std::size_t count_true(const std::array<bool, N>& a)
{
  std::size_t n = 0;

  for (bool b : a)
    n += b;

  return n;
}

inline std::string json_escape(std::string_view s)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;

  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += digits[c >> 4];
          out += digits[c & 0x0F];
        } else {
          out += static_cast<char>(c);
        }
    }
  }

  return out;
}

inline void write_error(webkit::response& res, const http_error& e)
{
  res.status(e.status());
  res.body("{\"error\":\"" + json_escape(e.what()) + "\"}", "application/json");
}

template<typename T>
struct async_value {
  using type = T;
};

template<typename T>
struct async_value<async::async<T>> {
  using type = T;
};

/**
 * @brief Everything a mount keeps alive: services, validators and interceptors, shared by its routes
 */
template<typename Ctx, typename... Args>
class mount_state {
public:
  using context_type = Ctx;

  template<typename... A>
  explicit mount_state(A&&... args)
      : items_(std::forward<A>(args)...)
  {}

  template<typename R>
  static async::async<void> handle(std::shared_ptr<mount_state> self, Ctx& ctx)
  {
    co_await self->template intercept<R, 0>(ctx);
  }

private:
  static constexpr std::size_t arg_count = sizeof...(Args);
  using arg_types = typelist<Args...>;

  static constexpr std::size_t validators_index =
      first_true(std::array<bool, arg_count + 1> {is_validator_set<Args>::value..., false});
  static constexpr std::size_t interceptors_index =
      first_true(std::array<bool, arg_count + 1> {is_interceptor_set<Args>::value..., false});

  static_assert(count_true(std::array<bool, arg_count + 1> {is_validator_set<Args>::value..., false}) <= 1,
                "mount takes at most one validators(...)");
  static_assert(count_true(std::array<bool, arg_count + 1> {is_interceptor_set<Args>::value..., false}) <= 1,
                "mount takes at most one interceptors(...)");

  auto& validator_items()
  {
    if constexpr (validators_index < arg_count)
      return std::get<validators_index>(items_).items;
    else
      return empty_;
  }

  auto& interceptor_items()
  {
    if constexpr (interceptors_index < arg_count)
      return std::get<interceptors_index>(items_).items;
    else
      return empty_;
  }

  template<typename R, std::size_t I>
  async::async<void> intercept(Ctx& ctx)
  {
    auto& items = interceptor_items();

    if constexpr (I == std::tuple_size_v<std::remove_reference_t<decltype(items)>>) {
      co_await call<R>(ctx);
    } else {
      std::optional<http_error> error;

      try {
        co_await std::get<I>(items)(call_info<R> {}, ctx,
                                    next_handler([this, &ctx]() { return intercept<R, I + 1>(ctx); }));
      } catch (const http_error& e) {
        error = e;
      }

      // Converted here so that the outer interceptors see the error response
      if (error)
        write_error(ctx.response(), *error);
    }
  }

  template<typename R>
  async::async<void> call(Ctx& ctx)
  {
    std::optional<http_error> error;

    try {
      co_await authorize<R, 0>(ctx);

      const webkit::query_map query = ctx.request().query();
      auto values = decode_args<R>(ctx, query, std::make_index_sequence<R::args::size>());
      co_await invoke<R>(ctx, std::move(values));
    } catch (const http_error& e) {
      error = e;
    }

    // Converted here so that the interceptors see the error response
    if (error)
      write_error(ctx.response(), *error);
  }

  //
  // Security
  //

  template<typename R, std::size_t I>
  async::async<void> authorize(Ctx& ctx)
  {
    if constexpr (I < R::schemes::size) {
      using scheme = at_t<I, typename R::schemes>;
      co_await validate<scheme>(ctx);
      co_await authorize<R, I + 1>(ctx);
    }
  }

  template<typename S>
  static std::optional<scheme_value_t<S>> scheme_value_of(Ctx& ctx)
  {
    const webkit::request& req = ctx.request();

    if constexpr (std::is_same_v<S, bearer_auth>) {
      auto value = req.headers().get("Authorization");
      return value ? param_traits<bearer>::parse(*value) : std::nullopt;
    } else if constexpr (S::in_query) {
      auto query = req.query();
      auto it = query.find(S::name.view());

      if (it == query.end())
        return std::nullopt;

      return api_key_value {S::name.view(), it->second};
    } else {
      auto value = req.headers().get(S::name.view());

      if (!value)
        return std::nullopt;

      return api_key_value {S::name.view(), std::string(*value)};
    }
  }

  template<typename S>
  async::async<void> validate(Ctx& ctx)
  {
    using value_type = scheme_value_t<S>;
    auto& items = validator_items();
    using items_type = std::remove_reference_t<decltype(items)>;

    constexpr std::size_t index = []<std::size_t... Is>(std::index_sequence<Is...>) {
      return first_true(std::array<bool, sizeof...(Is) + 1> {
          validates_v<std::tuple_element_t<Is, items_type>, value_type, Ctx>..., false});
    }(std::make_index_sequence<std::tuple_size_v<items_type>>());

    static_assert(index < std::tuple_size_v<items_type>,
                  "no validator for a security scheme of the api: pass rest::validators(...) to mount, "
                  "with a callable taking api_key_value or bearer (and optionally the context)");

    std::optional<value_type> value = scheme_value_of<S>(ctx);

    if (!value)
      throw http_error(webkit::status::unauthorized, "missing credentials");

    auto& validator = std::get<index>(items);
    bool accepted = false;

    if constexpr (std::is_invocable_v<decltype(validator), value_type, Ctx&>)
      accepted = co_await as_async(validator(std::move(*value), ctx));
    else
      accepted = co_await as_async(validator(std::move(*value)));

    if (!accepted)
      throw http_error(webkit::status::unauthorized, "invalid credentials");
  }

  template<typename T>
  static async::async<bool> as_async(T value)
  {
    if constexpr (async::is_async_v<T>)
      co_return static_cast<bool>(co_await std::move(value));
    else
      co_return static_cast<bool>(value);
  }

  //
  // Arguments
  //

  template<typename R, std::size_t... Is>
  static auto decode_args(Ctx& ctx, const webkit::query_map& query, std::index_sequence<Is...>)
  {
    // Braced initialization: decoded in order
    return apply_t<std::tuple, typename R::values> {decode_arg<at_t<Is, typename R::args>>(ctx, query)...};
  }

  template<typename T>
  static T parse_param(std::string_view raw, std::string_view what, std::string_view name)
  {
    auto value = param_traits<T>::parse(raw);

    if (!value)
      throw http_error(webkit::status::bad_request, "invalid " + std::string(what) + " '" + std::string(name) + "'");

    return std::move(*value);
  }

  template<typename D>
  static typename D::value_type decode_arg(Ctx& ctx, const webkit::query_map& query)
  {
    using T = typename D::value_type;

    if constexpr (requires { D::index; }) { // path_arg
      auto value = D::template_type::template parse<D::index>(ctx.param(D::index));

      if (!value)
        throw http_error(webkit::status::bad_request, "invalid path parameter '" + std::string(D::name) + "'");

      return std::move(*value);
    } else if constexpr (is_body<D>::value) {
      using codec = resolve_codec_t<T, typename D::codec>;
      auto content_type = ctx.request().headers().get("Content-Type");

      if (content_type && !same_media_type(*content_type, codec::content_type))
        throw http_error(webkit::status::unsupported_media_type, "expected " + std::string(codec::content_type));

      try {
        return codec::template decode<T>(ctx.request().body());
      } catch (const std::exception& e) {
        throw http_error(webkit::status::bad_request, std::string("invalid body: ") + e.what());
      }
    } else if constexpr (requires { D::name; }) { // query or header
      using element = param_element_t<T>;
      constexpr bool is_query = !std::is_same_v<D, header<D::name, T>>;
      constexpr std::string_view what = is_query ? "query parameter" : "header";
      std::string_view name = D::name.view();

      if constexpr (is_vector<T>::value) {
        T values;

        for (auto [it, end] = query.equal_range(name); it != end; ++it)
          values.push_back(parse_param<element>(it->second, what, name));

        return values;
      } else {
        std::optional<std::string_view> raw;

        if constexpr (is_query) {
          auto it = query.find(name);

          if (it != query.end())
            raw = it->second;
        } else {
          raw = ctx.request().headers().get(name);
        }

        if constexpr (is_optional<T>::value) {
          if (!raw)
            return std::nullopt;

          return parse_param<element>(*raw, what, name);
        } else {
          if (!raw)
            throw http_error(webkit::status::bad_request, "missing " + std::string(what) + " '" + std::string(name) + "'");

          return parse_param<element>(*raw, what, name);
        }
      }
    }
  }

  //
  // Service call
  //

  template<typename R>
  static constexpr std::size_t service_index()
  {
    constexpr std::array<bool, arg_count + 1> candidates = {
        (is_service_v<Args> && implements<Args, R, Ctx>::value)..., false};

    static_assert(count_true(candidates) != 0,
                  "an endpoint of the api is not implemented by any service: expected "
                  "operator()(Endpoint, path params..., parts..., [context&])");
    static_assert(count_true(candidates) < 2, "an endpoint of the api is implemented by several services");

    return first_true(candidates);
  }

  template<typename R, typename Values>
  async::async<void> invoke(Ctx& ctx, Values values)
  {
    using E = typename R::endpoint;
    using returns = typename R::returns;
    using expected = typename R::result_type;

    auto& service = std::get<service_index<R>()>(items_);
    using service_type = std::remove_reference_t<decltype(service)>;

    auto call = [&](auto&&... args) {
      if constexpr (std::is_invocable_v<service_type&, E, decltype(args)..., Ctx&>)
        return service(E {}, std::move(args)..., ctx);
      else
        return service(E {}, std::move(args)...);
    };

    using call_result = decltype(std::apply(call, std::move(values)));
    using value_type = typename async_value<call_result>::type;

    webkit::response& res = ctx.response();

    if constexpr (std::is_void_v<expected>) {
      static_assert(std::is_void_v<value_type>, "the endpoint declares no returns<> but the service returns a value");

      if constexpr (async::is_async_v<call_result>)
        co_await std::apply(call, std::move(values));
      else
        std::apply(call, std::move(values));

      res.status(returns::status);
    } else {
      static_assert(std::is_convertible_v<value_type, expected>,
                    "the service result does not match the returns<> of the endpoint");
      using codec = resolve_codec_t<expected, typename returns::codec>;

      if constexpr (async::is_async_v<call_result>) {
        expected result = co_await std::apply(call, std::move(values));
        res.body(codec::encode(result), std::string(codec::content_type));
      } else {
        expected result = std::apply(call, std::move(values));
        res.body(codec::encode(result), std::string(codec::content_type));
      }

      res.status(returns::status);
    }
  }

private:
  std::tuple<Args...> items_;
  std::tuple<> empty_;
};

template<typename Api, typename Server, typename State, typename... Rs>
void register_routes(Server& server, const std::shared_ptr<State>& state, typelist<Rs...>)
{
  using context_type = typename Server::context_type;

  (server.route(std::string(method_name(Rs::method)), Rs::path_type::regex(),
                [state](context_type& ctx) { return State::template handle<Rs>(state, ctx); }),
   ...);
}

} // namespace detail

/**
 * @brief Serves Api on a webkit server.
 *
 *   rest::mount<bank::v1>(server,
 *                         auth_service{db}, users_service{db}, cards_service{db},
 *                         rest::validators(check_api_key),
 *                         rest::interceptors(audit{}));
 *
 * Every endpoint must be implemented by exactly one service, as operator()(Endpoint, args...) with the
 * arguments of the endpoint (path parameters, then its parts in order, then the inherited parts) and
 * optionally the context last, returning the returns<> type or async of it.
 *
 * Per request: webkit middlewares, interceptors, security validators, arguments decoding, service, encoding.
 * Invalid arguments answer 400, a wrong Content-Type 415, missing or rejected credentials 401, http_error its
 * status, any other exception 500.
 */
template<typename Api, typename Server, typename... Args>
void mount(Server& server, Args&&... args)
{
  using state_type = detail::mount_state<typename Server::context_type, std::decay_t<Args>...>;

  auto state = std::make_shared<state_type>(std::forward<Args>(args)...);
  detail::register_routes<Api>(server, state, typename Api::endpoints {});
}

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_SERVER_HPP
