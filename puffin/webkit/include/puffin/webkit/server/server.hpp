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
#include <puffin/webkit/http/parser.hpp>
#include <puffin/webkit/http/serializer.hpp>
#include <puffin/webkit/server/context.hpp>
#include <puffin/webkit/server/middleware.hpp>
#include <puffin/webkit/server/router.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <algorithm>
#include <concepts>
#include <exception>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace puffin {
namespace webkit {

struct server_options {
  parser_limits limits;
  std::size_t read_buffer_size = 16 * 1024;
};

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
    using stream_type = accepted_stream_t<A>;

    async::any_executor executor = co_await async::this_executor;

    for (;;) {
      std::optional<stream_type> stream;
      std::exception_ptr error;

      try {
        stream.emplace(co_await acceptor.accept());
      } catch (...) {
        error = std::current_exception();
      }

      if (error) {
        if (!acceptor.is_open())
          co_return;

        std::rethrow_exception(error);
      }

      async::co_spawn(executor, [this, s = std::move(*stream)]() mutable { return serve(std::move(s)); });
    }
  }

  /**
   * @brief Serves the requests of one connection until it is closed by either side.
   *        Transport errors end the connection silently.
   */
  template<Stream S>
  async::async<void> serve(S stream)
  {
    request_parser parser(options_.limits);
    std::vector<char> buffer(options_.read_buffer_size);
    std::string pending; // Bytes received after the current request (pipelining)

    try {
      for (;;) {
        parse_status status = parse_status::need_more;

        if (!pending.empty()) {
          auto result = parser.feed(pending);
          pending.erase(0, result.consumed);
          status = result.status;
        }

        while (status == parse_status::need_more) {
          std::size_t n = co_await stream.read_some(std::span<char>(buffer));

          if (n == 0) { // Closed by the client
            stream.close();
            co_return;
          }

          auto result = parser.feed(std::string_view(buffer.data(), n));

          if (result.status == parse_status::done)
            pending.append(buffer.data() + result.consumed, n - result.consumed);

          status = result.status;
        }

        response res;
        bool keep_alive = false;
        bool head = false;

        if (status == parse_status::error) {
          res = error_response(parser.error());
        } else {
          request req = parser.release();
          head = req.method() == "HEAD";
          keep_alive = req.keep_alive();

          co_await handle(req, res);

          if (res.headers().has_token("Connection", "close"))
            keep_alive = false;
          else if (keep_alive && req.version() < http_1_1)
            res.headers().set("Connection", "keep-alive");
        }

        if (!keep_alive)
          res.headers().set("Connection", "close");

        std::string wire = serialize(res);

        if (head) // Same headers as GET, without the body
          wire.resize(wire.find("\r\n\r\n") + 4);

        co_await stream.write(std::span<const char>(wire));

        if (!keep_alive) {
          stream.close();
          co_return;
        }
      }
    } catch (...) {
      stream.close();
    }
  }

  /**
   * @brief Runs the middlewares and the matching route for a request, without any I/O
   */
  async::async<void> handle(request& req, response& res)
  {
    context_type ctx(req, res);
    std::size_t entered = 0;
    bool failed = false;

    try {
      if (chain_.before(ctx, entered)) {
        auto match = router_.find(req.method(), req.path());

        if (match) {
          ctx.params(std::move(match.params));
          co_await (*match.handler)(ctx);
        } else if (!match.allowed_methods.empty()) {
          res.status(status::method_not_allowed);
          res.headers().set("Allow", join(match.allowed_methods));
        } else {
          res.status(status::not_found);
        }
      }
    } catch (...) {
      failed = true;
    }

    if (failed)
      res = response(status::internal_server_error);

    try {
      chain_.after(ctx, entered);
    } catch (...) {
      res = response(status::internal_server_error);
    }
  }

private:
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

  static std::string join(const std::vector<std::string>& values)
  {
    std::string s;

    for (std::size_t i = 0; i < values.size(); ++i) {
      if (std::find(values.begin(), values.begin() + i, values[i]) != values.begin() + i)
        continue;

      if (!s.empty())
        s += ", ";

      s += values[i];
    }

    return s;
  }

  static response error_response(parse_error e)
  {
    switch (e) {
      case parse_error::header_too_large: return response(status::request_header_fields_too_large);
      case parse_error::body_too_large: return response(status::payload_too_large);
      case parse_error::uri_too_long: return response(status::uri_too_long);
      default: return response(status::bad_request);
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
