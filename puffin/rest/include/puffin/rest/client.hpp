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
#include <puffin/async/try_await.hpp>
#include <puffin/rest/codec.hpp>
#include <puffin/rest/detail/arguments.hpp>
#include <puffin/rest/detail/client.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/rest/transport.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/client/interceptor.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <concepts>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {

template<typename Client, typename Api, typename... Fixed>
class scoped_client;

/**
 * @brief Client of Api, over any_transport: a webkit Connector with host and port, or any Transport:
 *
 *   rest::client<bank::v1, webkit::interceptors::retry> api(connector, "bank.local", 8080);
 *   api.credentials<rest::api_key<"X-Api-Key">>("sk_...");
 *   user u = co_await api.call<bank::users::get>(42);
 *
 * Arguments of call are those of the endpoint: path parameters, then its parts in order, then the inherited
 * parts. Trailing optional arguments may be omitted. Security schemes are not arguments: the client sends the
 * credentials given for the schemes of each endpoint. Interceptors are webkit ones (see
 * webkit::interceptor_chain), run around the transport.
 *
 * Like webkit::basic_client, one call at a time.
 */
template<typename Api, typename... Interceptors>
class client {
public:
  using api_type = Api;

  template<typename E>
  using result_type = typename find_endpoint_t<Api, E>::result_type;

  /// Arguments are given to any_transport: a Connector, host and port, or a Transport
  template<typename... A>
    requires std::constructible_from<any_transport, A...>
  explicit client(A&&... args)
      : transport_(std::forward<A>(args)...)
  {}

  client(const client&) = delete;
  client& operator=(const client&) = delete;

  /// Headers added to every request (User-Agent...)
  webkit::headers& headers() noexcept { return headers_; }

  template<typename I>
  I& interceptor()
  {
    return interceptors_.template get<I>();
  }

  /// The credential sent for security scheme S (api_key<>, api_key_query<>, bearer_auth) by the endpoints using it
  template<typename S>
  void credentials(std::string value)
  {
    static_assert(std::is_base_of_v<detail::scheme_tag, S>, "credentials are given for a security scheme");
    credentials_.template set<S>(std::move(value));
  }

  /**
   * @brief Calls endpoint E, throws http_error if the response is not 2xx. The arguments are copied into the task,
   *        every error (encoding, transport, status) comes out of it.
   */
  template<typename E, typename... A>
  async::async<result_type<E>> call(A... args)
  {
    using R = find_endpoint_t<Api, E>;
    co_return co_await send<R>(build_request<R>(std::move(args)...));
  }

  /// Calls endpoint E, an error status is returned instead of thrown (encoding and transport errors are still thrown)
  template<typename E, typename... A>
  async::async<result<result_type<E>>> try_call(A... args)
  {
    using R = find_endpoint_t<Api, E>;
    webkit::request req = build_request<R>(std::move(args)...);
    auto outcome = co_await async::try_await(send<R>(std::move(req)));
    co_return detail::to_result<result_type<E>>(std::move(outcome));
  }

  /**
   * @brief A view on a part of the api with its leading arguments fixed:
   *
   *   auto ada_cards = api.scope<bank::cards::api>(ada.id);
   *   auto all = co_await ada_cards.call<bank::cards::list>();
   */
  template<typename SubApi, typename... A>
  scoped_client<client, SubApi, std::decay_t<A>...> scope(A&&... fixed)
  {
    return scoped_client<client, SubApi, std::decay_t<A>...>(*this, std::forward<A>(fixed)...);
  }

  /// The request call<E>(args...) would send, before the interceptors
  template<typename E, typename... A>
  webkit::request make_request(A&&... args) const
  {
    return build_request<find_endpoint_t<Api, E>>(std::forward<A>(args)...);
  }

private:
  template<typename R, typename... A>
  webkit::request build_request(A&&... args) const
  {
    detail::outgoing out = detail::encode_parts<R>(detail::complete_args<R>(std::forward<A>(args)...), headers_);
    credentials_.template apply<R>(out);

    webkit::request req = detail::to_request<R>(std::move(out));
    using expected = typename R::result_type;

    if constexpr (!std::is_void_v<expected>) {
      using codec = detail::result_codec_t<R>;

      if (!req.headers().contains("Accept"))
        req.headers().set("Accept", std::string(codec::content_type));
    }

    return req;
  }

  async::async<webkit::response> transmit(webkit::request req)
  {
    return interceptors_.run(std::move(req), [this](webkit::request r) { return transport_.request(std::move(r)); });
  }

  template<typename R>
  async::async<typename R::result_type> send(webkit::request req)
  {
    using expected = typename R::result_type;

    webkit::response res = co_await transmit(std::move(req));

    if (res.status_code() < 200 || res.status_code() >= 300)
      throw http_error(res.status_code(), std::move(res.body()), std::move(res.headers()));

    if constexpr (!std::is_void_v<expected>) {
      using codec = detail::result_codec_t<R>;
      co_return codec::template decode<expected>(res.body());
    }
  }

private:
  any_transport transport_;
  webkit::headers headers_;
  detail::credential_store credentials_;
  webkit::interceptor_chain<Interceptors...> interceptors_;
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
    static_assert(Api::template contains<E>, "this endpoint is not part of the scope");

    return std::apply([&](const Fixed&... fixed) { return parent_.template call<E>(fixed..., std::forward<A>(args)...); },
                      fixed_);
  }

  template<typename E, typename... A>
  auto try_call(A&&... args)
  {
    static_assert(Api::template contains<E>, "this endpoint is not part of the scope");

    return std::apply(
        [&](const Fixed&... fixed) { return parent_.template try_call<E>(fixed..., std::forward<A>(args)...); }, fixed_);
  }

private:
  Client& parent_;
  std::tuple<Fixed...> fixed_;
};

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_CLIENT_HPP
