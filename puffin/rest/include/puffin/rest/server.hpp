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
#include <puffin/rest/detail/arguments.hpp>
#include <puffin/rest/detail/security.hpp>
#include <puffin/rest/detail/service.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/webkit/server/server.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {

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

namespace detail {

template<typename T>
struct is_validator_set : std::false_type {};

template<typename... V>
struct is_validator_set<validator_set<V...>> : std::true_type {};

template<typename T>
inline constexpr bool is_service_v = !is_validator_set<T>::value;

/// Index of the validators(...) argument of mount, the count if there is none
template<typename... Args>
constexpr std::size_t validators_index()
{
  constexpr std::array<bool, sizeof...(Args) + 1> found = {is_validator_set<Args>::value..., false};
  static_assert(count_true(found) <= 1, "mount takes at most one validators(...)");
  return first_true(found);
}

/// The service of mount implementing endpoint R, checked at compile time
template<typename R, typename Ctx, typename... Args>
constexpr std::size_t service_index()
{
  constexpr std::array<bool, sizeof...(Args) + 1> candidates = {
      (is_service_v<Args> && implements<Args, R, Ctx>::value)..., false};

  static_assert(count_true(candidates) != 0,
                "an endpoint of the api is not implemented by any service: expected "
                "operator()(Endpoint, path params..., parts..., [context&])");
  static_assert(count_true(candidates) < 2, "an endpoint of the api is implemented by several services");

  return first_true(candidates);
}

/**
 * @brief Everything a mount keeps alive: services and validators, shared by its routes.
 *
 * A call goes through security, argument decoding and the service. An http_error becomes the response, so that
 * the webkit middlewares around the route see it.
 */
template<typename Ctx, typename... Args>
class mount_state {
public:
  template<typename... A>
  explicit mount_state(A&&... args)
      : items_(std::forward<A>(args)...)
  {}

  /// Handles a request to endpoint R, an http_error becomes the response; the route owning this state outlives it
  template<typename R>
  async::async<void> handle(Ctx& ctx)
  {
    return answer_errors(ctx, call<R>(ctx));
  }

private:
  auto& validators()
  {
    constexpr std::size_t index = validators_index<Args...>();

    if constexpr (index < sizeof...(Args))
      return std::get<index>(items_).items;
    else
      return empty_;
  }

  template<typename R>
  async::async<void> call(Ctx& ctx)
  {
    co_await authorize<R>(validators(), ctx);

    auto& service = std::get<service_index<R, Ctx, Args...>()>(items_);

    if constexpr (std::is_void_v<typename R::result_type>) {
      co_await call_service<R>(service, ctx, decode_args<R>(ctx));
      write_result<R>(ctx.response());
    } else {
      auto result = co_await call_service<R>(service, ctx, decode_args<R>(ctx));
      write_result<R>(ctx.response(), result);
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

  static_assert(((Api::template count<typename Rs::endpoint> == 1) && ...),
                "an endpoint appears several times in the api");

  (server.route(
       std::string(method_name(Rs::method)), Rs::path_type::regex(),
       [state](context_type& ctx) { return state->template handle<Rs>(ctx); }, std::string(Rs::name)),
   ...);
}

} // namespace detail

/**
 * @brief Serves Api on a webkit server.
 *
 *   rest::mount<bank::v1>(server,
 *                         auth_service{db}, users_service{db}, cards_service{db},
 *                         rest::validators(check_api_key));
 *
 * Every endpoint must be implemented by exactly one service, as operator()(Endpoint, args...) with the
 * arguments of the endpoint (path parameters, then its parts in order, then the inherited parts) and
 * optionally the context last, returning the returns<> type or async of it.
 *
 * Each route is named after its endpoint ("users.get"), reachable by webkit middlewares with ctx.route()->name.
 * Per request: webkit middlewares, security validators, arguments decoding, service, encoding.
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
