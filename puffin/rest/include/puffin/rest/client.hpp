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

#ifndef PUFFIN_REST_CLIENT_HPP
#define PUFFIN_REST_CLIENT_HPP

#include <puffin/async.hpp>
#include <puffin/rest/codec.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {

/**
 * @brief What a rest client sends its requests through: webkit::basic_client, local_transport...
 */
template<typename T>
concept Transport = requires(T& t, webkit::request req) {
  { t.request(std::move(req)) } -> std::same_as<async::async<webkit::response>>;
};

/**
 * @brief A transport calling a webkit server in process, without any I/O. For tests, or to call an api
 *        served by the same program.
 */
template<typename Server>
class local_transport {
public:
  explicit local_transport(Server& server)
      : server_(server)
  {}

  async::async<webkit::response> request(webkit::request req)
  {
    webkit::response res;
    co_await server_.handle(req, res);
    co_return res;
  }

private:
  Server& server_;
};

/// Continues a client call: the next interceptor, or the transport
class next_request {
public:
  explicit next_request(std::function<async::async<webkit::response>(webkit::request&)> next)
      : next_(std::move(next))
  {}

  async::async<webkit::response> operator()(webkit::request& req) const { return next_(req); }

private:
  std::function<async::async<webkit::response>(webkit::request&)> next_;
};

namespace detail {

template<typename T>
struct transport_for {
  using type = T;
};

template<webkit::Connector C>
struct transport_for<C> {
  using type = webkit::basic_client<C>;
};

/// Number of trailing optional arguments, which callers may omit
template<typename Values>
struct trailing_optionals;

template<>
struct trailing_optionals<typelist<>> : std::integral_constant<std::size_t, 0> {};

template<typename V, typename... Vs>
struct trailing_optionals<typelist<V, Vs...>>
    : std::integral_constant<std::size_t, (trailing_optionals<typelist<Vs...>>::value == sizeof...(Vs) &&
                                           is_optional<V>::value)
                                              ? sizeof...(Vs) + 1
                                              : trailing_optionals<typelist<Vs...>>::value> {};

inline void append_query(std::string& target, std::string_view name, const std::string& value)
{
  target += target.find('?') == std::string::npos ? '?' : '&';
  target += webkit::detail::percent_encode(name);
  target += '=';
  target += webkit::detail::percent_encode(value);
}

template<typename Schemes>
struct for_each_scheme;

template<typename... S>
struct for_each_scheme<typelist<S...>> {
  template<typename F>
  static void apply(F&& f)
  {
    (f.template operator()<S>(), ...);
  }
};

} // namespace detail

template<typename Client, typename Api, typename... Fixed>
class scoped_client;

/**
 * @brief Client of Api, over a Transport or a webkit Connector (wrapped in a webkit::basic_client):
 *
 *   rest::client<bank::v1, webkit::asio::tcp_connector, rest::intercept::api_key> api(connector, "bank.local", 8080);
 *   api.interceptor<rest::intercept::api_key>().key("sk_...");
 *   user u = co_await api.call<bank::users::get>(42);
 *
 * Arguments of call are those of the endpoint: path parameters, then its parts in order, then the inherited
 * parts. Trailing optional arguments may be omitted. Security schemes are not arguments, interceptors fill
 * them. Interceptors run from the first to the last around the transport:
 *
 *   struct trace {
 *     template<typename Info>
 *     async<webkit::response> operator()(Info, webkit::request& req, rest::next_request next);
 *   };
 *
 * Like webkit::basic_client, one call at a time.
 */
template<typename Api, typename T, typename... Interceptors>
class client {
public:
  using api_type = Api;
  using transport_type = typename detail::transport_for<T>::type;

  static_assert(Transport<transport_type>, "client needs a Transport or a webkit Connector");

  template<typename E, typename Scope = Api>
  using result_type = typename find_endpoint_t<Api, E, Scope>::result_type;

  /// Arguments are given to the transport (for a Connector: the connector, host and port)
  template<typename... A>
    requires std::constructible_from<transport_type, A...>
  explicit client(A&&... args)
      : transport_(std::forward<A>(args)...)
  {}

  client(const client&) = delete;
  client& operator=(const client&) = delete;

  transport_type& transport() noexcept { return transport_; }

  /// Headers added to every request (User-Agent...)
  webkit::headers& headers() noexcept { return headers_; }

  template<typename I>
  I& interceptor()
  {
    return std::get<I>(interceptors_);
  }

  /// Calls endpoint E (looked up in the part Scope of the api), throws http_error if the response is not 2xx
  template<typename E, typename Scope = Api, typename... A>
  async::async<result_type<E, Scope>> call(A&&... args)
  {
    using R = find_endpoint_t<Api, E, Scope>;
    return send<R>(build_request<R>(std::forward<A>(args)...));
  }

  /// Calls endpoint E, an error status is returned instead of thrown (transport errors are still thrown)
  template<typename E, typename Scope = Api, typename... A>
  async::async<result<result_type<E, Scope>>> try_call(A&&... args)
  {
    using R = find_endpoint_t<Api, E, Scope>;
    return try_send<R>(build_request<R>(std::forward<A>(args)...));
  }

  /**
   * @brief A view on a part of the api, endpoints are looked up there, with its leading arguments fixed:
   *
   *   auto ada_cards = api.scope<bank::cards::api>(ada.id);
   *   auto all = co_await ada_cards.call<bank::cards::list>();
   *   co_await api.scope<bank::users::api>().call<bank::users::remove>(id);
   */
  template<typename SubApi, typename... A>
  scoped_client<client, SubApi, std::decay_t<A>...> scope(A&&... fixed)
  {
    return scoped_client<client, SubApi, std::decay_t<A>...>(*this, std::forward<A>(fixed)...);
  }

  /// The request call<E>(args...) would send, before interceptors
  template<typename E, typename Scope = Api, typename... A>
  webkit::request make_request(A&&... args) const
  {
    return build_request<find_endpoint_t<Api, E, Scope>>(std::forward<A>(args)...);
  }

private:
  template<typename R, typename... A>
  webkit::request build_request(A&&... args) const
  {
    using values = typename R::values;
    constexpr std::size_t count = values::size;
    constexpr std::size_t required = count - detail::trailing_optionals<values>::value;

    static_assert(sizeof...(A) <= count, "too many arguments for this endpoint");
    static_assert(sizeof...(A) >= required, "missing arguments for this endpoint");

    auto given = std::forward_as_tuple(std::forward<A>(args)...);
    auto full = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return detail::apply_t<std::tuple, values> {argument<detail::at_t<Is, values>, Is>(given)...};
    }(std::make_index_sequence<count>());

    webkit::request req(std::string(method_name(R::method)), std::string());

    for (const auto& h : headers_)
      req.headers().add(h.name, h.value);

    std::string target = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return R::path_type::format(std::get<Is>(full)...);
    }(std::make_index_sequence<R::path_type::param_count>());

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (encode_arg<detail::at_t<Is, typename R::args>>(req, target, std::get<Is>(full)), ...);
    }(std::make_index_sequence<count>());

    req.target(std::move(target));

    using expected = typename R::result_type;

    if constexpr (!std::is_void_v<expected>) {
      using codec = detail::resolve_codec_t<expected, typename R::returns::codec>;

      if (!req.headers().contains("Accept"))
        req.headers().set("Accept", std::string(codec::content_type));
    }

    return req;
  }

  template<typename V, std::size_t I, typename Given>
  static V argument(Given& given)
  {
    if constexpr (I < std::tuple_size_v<Given>) {
      using A = std::tuple_element_t<I, Given>;
      static_assert(std::is_constructible_v<V, A>, "argument type does not match the endpoint");
      return V(std::forward<A>(std::get<I>(given)));
    } else {
      return V {};
    }
  }

  template<typename D, typename V>
  static void encode_arg(webkit::request& req, std::string& target, const V& value)
  {
    using element = detail::param_element_t<V>;

    if constexpr (requires { D::index; }) {
      // Path parameters are already in the target
    } else if constexpr (detail::is_body<D>::value) {
      using codec = detail::resolve_codec_t<V, typename D::codec>;
      req.body(codec::encode(value));
      req.headers().set("Content-Type", std::string(codec::content_type));
    } else if constexpr (std::is_same_v<D, header<D::name, V>>) {
      if constexpr (detail::is_optional<V>::value) {
        if (value)
          req.headers().set(std::string(D::name.view()), param_traits<element>::format(*value));
      } else {
        req.headers().set(std::string(D::name.view()), param_traits<element>::format(value));
      }
    } else { // query
      if constexpr (detail::is_vector<V>::value) {
        for (const auto& v : value)
          detail::append_query(target, D::name.view(), param_traits<element>::format(v));
      } else if constexpr (detail::is_optional<V>::value) {
        if (value)
          detail::append_query(target, D::name.view(), param_traits<element>::format(*value));
      } else {
        detail::append_query(target, D::name.view(), param_traits<element>::format(value));
      }
    }
  }

  template<typename R, std::size_t I>
  async::async<webkit::response> intercept(webkit::request& req)
  {
    if constexpr (I == sizeof...(Interceptors)) {
      // A copy: an interceptor may send the request again
      co_return co_await transport_.request(req);
    } else {
      co_return co_await std::get<I>(interceptors_)(
          call_info<R> {}, req, next_request([this](webkit::request& r) { return intercept<R, I + 1>(r); }));
    }
  }

  template<typename R>
  async::async<typename R::result_type> send(webkit::request req)
  {
    using expected = typename R::result_type;

    webkit::response res = co_await intercept<R, 0>(req);

    if (res.status_code() < 200 || res.status_code() >= 300)
      throw http_error(res.status_code(), std::move(res.body()), std::move(res.headers()));

    if constexpr (!std::is_void_v<expected>) {
      using codec = detail::resolve_codec_t<expected, typename R::returns::codec>;
      co_return codec::template decode<expected>(res.body());
    }
  }

  template<typename R>
  async::async<result<typename R::result_type>> try_send(webkit::request req)
  {
    using expected = typename R::result_type;
    std::optional<http_error> error;

    try {
      if constexpr (std::is_void_v<expected>) {
        co_await send<R>(std::move(req));
        co_return result<void>();
      } else {
        co_return result<expected>(co_await send<R>(std::move(req)));
      }
    } catch (const http_error& e) {
      error = e;
    }

    co_return result<expected>(std::move(*error));
  }

private:
  transport_type transport_;
  webkit::headers headers_;
  std::tuple<Interceptors...> interceptors_;
};

template<typename Client, typename Api, typename... Fixed>
class scoped_client {
public:
  template<typename... A>
  explicit scoped_client(Client& parent, A&&... fixed)
      : parent_(parent), fixed_(std::forward<A>(fixed)...)
  {}

  template<typename E, typename... A>
  auto call(A&&... args)
  {
    return std::apply(
        [&](const Fixed&... fixed) { return parent_.template call<E, Api>(fixed..., std::forward<A>(args)...); },
        fixed_);
  }

  template<typename E, typename... A>
  auto try_call(A&&... args)
  {
    return std::apply(
        [&](const Fixed&... fixed) { return parent_.template try_call<E, Api>(fixed..., std::forward<A>(args)...); },
        fixed_);
  }

private:
  Client& parent_;
  std::tuple<Fixed...> fixed_;
};

namespace intercept {

/// Fills the api_key<> and api_key_query<> schemes of the called endpoints
class api_key {
public:
  void key(std::string key) { key_ = std::move(key); }
  const std::string& key() const noexcept { return key_; }

  template<typename Info>
  async::async<webkit::response> operator()(Info, webkit::request& req, next_request next)
  {
    detail::for_each_scheme<typename Info::schemes>::apply([&]<typename S>() {
      if constexpr (requires { S::in_query; }) {
        if constexpr (S::in_query) {
          std::string target = req.target();
          detail::append_query(target, S::name.view(), key_);
          req.target(std::move(target));
        } else {
          req.headers().set(std::string(S::name.view()), key_);
        }
      }
    });

    co_return co_await next(req);
  }

private:
  std::string key_;
};

/**
 * @brief Fills the bearer_auth schemes of the called endpoints. With a refresh function, a 401 refreshes the
 *        token and sends the request again, once.
 */
class bearer {
public:
  void token(std::string token) { token_ = std::move(token); }
  const std::string& token() const noexcept { return token_; }

  void on_refresh(std::function<async::async<std::string>()> refresh) { refresh_ = std::move(refresh); }

  template<typename Info>
  async::async<webkit::response> operator()(Info, webkit::request& req, next_request next)
  {
    if constexpr (!detail::contains_v<bearer_auth, typename Info::schemes>) {
      co_return co_await next(req);
    } else {
      req.headers().set("Authorization", "Bearer " + token_);
      webkit::response res = co_await next(req);

      if (res.status_code() != static_cast<int>(webkit::status::unauthorized) || !refresh_)
        co_return res;

      token_ = co_await refresh_();
      req.headers().set("Authorization", "Bearer " + token_);
      co_return co_await next(req);
    }
  }

private:
  std::string token_;
  std::function<async::async<std::string>()> refresh_;
};

/**
 * @brief Sends idempotent requests again when the transport fails or the server answers 502, 503 or 504.
 */
class retry {
public:
  /// Total number of attempts, 3 by default
  void attempts(std::size_t n) { attempts_ = n == 0 ? 1 : n; }
  std::size_t attempts() const noexcept { return attempts_; }

  template<typename Info>
  async::async<webkit::response> operator()(Info, webkit::request& req, next_request next)
  {
    if constexpr (!idempotent(Info::method)) {
      co_return co_await next(req);
    } else {
      for (std::size_t attempt = 1;; ++attempt) {
        const bool last = attempt >= attempts_;
        std::exception_ptr error;
        std::optional<webkit::response> res;

        try {
          res.emplace(co_await next(req));
        } catch (...) {
          error = std::current_exception();
        }

        if (error) {
          if (last)
            std::rethrow_exception(error);

          continue;
        }

        int code = res->status_code();

        if (last || (code != 502 && code != 503 && code != 504))
          co_return std::move(*res);
      }
    }
  }

private:
  std::size_t attempts_ = 3;
};

} // namespace intercept

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_CLIENT_HPP
