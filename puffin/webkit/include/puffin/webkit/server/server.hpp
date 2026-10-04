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

#ifndef PUFFIN_WEBKIT_SERVER_SERVER_HPP
#define PUFFIN_WEBKIT_SERVER_SERVER_HPP

#include <puffin/async.hpp>
#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/server/connection.hpp>
#include <puffin/webkit/server/context.hpp>
#include <puffin/webkit/server/middleware.hpp>
#include <puffin/webkit/server/router.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace puffin {
namespace webkit {

/**
 * @brief HTTP/1.1 server, written with coroutines over any transport satisfying the Stream and
 *        Acceptor concepts (see the asio and Qt adapters).
 *
 * Handlers take the context (request, response, route parameters, middleware data) and either
 * return async<void> or nothing:
 *
 *   server.get("/users/([0-9]+)", [](auto& ctx) -> async<void> {
 *     ctx.response().body(co_await load_user(ctx.param(0)), "application/json");
 *   });
 *
 * Routes and middlewares must be set up before serving. When connections are served from several
 * threads (multi-threaded executor), middlewares and handlers must be thread safe.
 */
template<typename... Middlewares>
class basic_server {
public:
  using context_type = basic_context<Middlewares...>;
  using handler_type = std::function<async::async<void>(context_type&)>;

  basic_server() = default;

  explicit basic_server(server_options options, Middlewares... middlewares)
      : options_(std::move(options)), chain_(std::move(middlewares)...)
  {}

  /// Adds a route, pattern being a regular expression matched against the whole path
  template<typename F>
    requires std::invocable<F&, context_type&>
  void route(std::string method, const std::string& pattern, F handler)
  {
    router_.add(std::move(method), pattern, make_handler(std::move(handler)));
  }

  template<typename F>
  void get(const std::string& pattern, F handler)
  {
    route("GET", pattern, std::move(handler));
  }

  template<typename F>
  void post(const std::string& pattern, F handler)
  {
    route("POST", pattern, std::move(handler));
  }

  template<typename F>
  void put(const std::string& pattern, F handler)
  {
    route("PUT", pattern, std::move(handler));
  }

  template<typename F>
  void patch(const std::string& pattern, F handler)
  {
    route("PATCH", pattern, std::move(handler));
  }

  template<typename F>
  void del(const std::string& pattern, F handler)
  {
    route("DELETE", pattern, std::move(handler));
  }

  /// Access to a middleware instance, to configure it
  template<typename M>
  M& middleware()
  {
    return chain_.template get<M>();
  }

  /**
   * @brief Accepts connections until the acceptor is closed, each connection is served by its own
   *        task on the executor of the calling coroutine
   */
  template<Acceptor A>
  async::async<void> listen(A& acceptor)
  {
    async::any_executor executor = co_await async::this_executor;

    while (auto stream = co_await accept(acceptor))
      async::co_spawn(executor, serve(std::move(*stream)));
  }

  /**
   * @brief Serves the requests of one connection until it is closed by either side.
   *        Transport errors end the connection silently.
   */
  template<Stream S>
  async::async<void> serve(S stream)
  {
    basic_connection<S, basic_server> connection(std::move(stream), options_, *this);
    co_await connection.run();
  }

  /**
   * @brief Runs the middlewares and the matching route for a request, without any I/O.
   *        Exceptions from middlewares and handlers become a 500.
   */
  async::async<void> handle(request& req, response& res)
  {
    context_type ctx(req, res);
    std::size_t entered = 0;

    try {
      if (chain_.before(ctx, entered))
        co_await dispatch(ctx);
    } catch (...) {
      fail(res);
    }

    try {
      chain_.after(ctx, entered);
    } catch (...) {
      fail(res);
    }
  }

private:
  /// The next connection, nullopt once the acceptor is closed
  template<Acceptor A>
  static async::async<std::optional<accepted_stream_t<A>>> accept(A& acceptor)
  {
    try {
      co_return co_await acceptor.accept();
    } catch (...) {
      if (acceptor.is_open())
        throw;
    }

    co_return std::nullopt;
  }

  /// Calls the route matching the request, or answers 404 / 405
  async::async<void> dispatch(context_type& ctx)
  {
    auto match = router_.find(ctx.request().method(), ctx.request().path());

    if (match) {
      ctx.params(std::move(match.params));
      co_await (*match.handler)(ctx);
    } else if (!match.allowed_methods.empty()) {
      ctx.response().status(status::method_not_allowed);
      ctx.response().headers().set("Allow", detail::join(match.allowed_methods, ", "));
    } else {
      ctx.response().status(status::not_found);
    }
  }

  /// Replaces the response after a middleware or handler failure
  static void fail(response& res) { res = response(status::internal_server_error); }

  /// Wraps synchronous handlers into coroutines, so the router stores a single handler type
  template<typename F>
  static handler_type make_handler(F handler)
  {
    using result_type = std::invoke_result_t<F&, context_type&>;

    if constexpr (async::is_async_v<result_type>) {
      static_assert(std::is_void_v<typename result_type::value_type>, "async handlers must return async<void>");
      return handler_type(std::move(handler));
    } else {
      return [handler = std::move(handler)](context_type& ctx) mutable -> async::async<void> {
        handler(ctx);
        co_return;
      };
    }
  }

private:
  server_options options_;
  middleware_chain<Middlewares...> chain_;
  basic_router<handler_type> router_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_SERVER_SERVER_HPP
