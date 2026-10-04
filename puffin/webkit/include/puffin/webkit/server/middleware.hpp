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

#ifndef PUFFIN_WEBKIT_SERVER_MIDDLEWARE_HPP
#define PUFFIN_WEBKIT_SERVER_MIDDLEWARE_HPP

#include <puffin/async/async.hpp>
#include <puffin/webkit/detail/continuation.hpp>
#include <puffin/webkit/server/context.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace webkit {

/// Continues a request from an around() middleware: the next around() middleware, or the route handler
using next_handler = detail::continuation<async::async<void>()>;

/**
 * @brief A middleware may define:
 *  - a nested data_type, added to the context (see middleware_data),
 *  - before(Context&), called before the route handler. Returning false stops the request: the remaining
 *    middlewares and the handler are skipped, the response is sent as is,
 *  - async<void> around(Context&, next_handler), called in order once every before() ran, around the route
 *    handler. It may answer the request itself, or co_await next() and then look at the response,
 *  - after(Context&), called after the route handler, in reverse order.
 * All of them are optional.
 */
template<typename... Middlewares>
class middleware_chain {
public:
  using context_type = basic_context<Middlewares...>;

  middleware_chain() = default;

  explicit middleware_chain(Middlewares... middlewares)
    requires(sizeof...(Middlewares) > 0)
      : middlewares_(std::move(middlewares)...)
  {}

  template<typename M>
  M& get()
  {
    return std::get<M>(middlewares_);
  }

  /**
   * @brief Runs before() of each middleware in order.
   * @param entered receives the number of middlewares whose before() ran, to pass to after()
   * @return false if a middleware stopped the request
   */
  bool before(context_type& ctx, std::size_t& entered)
  {
    entered = 0;
    return before_impl(ctx, entered, std::index_sequence_for<Middlewares...>{});
  }

  /// Runs after() of the first `entered` middlewares, in reverse order
  void after(context_type& ctx, std::size_t entered)
  {
    after_impl(ctx, entered, std::index_sequence_for<Middlewares...>{});
  }

  /// Runs the around() middlewares in order, the last one continuing with handler()
  template<typename Handler>
  async::async<void> around(context_type& ctx, Handler& handler)
  {
    return around_from<0>(ctx, handler);
  }

private:
  template<std::size_t I, typename Handler>
  async::async<void> around_from(context_type& ctx, Handler& handler)
  {
    if constexpr (I == sizeof...(Middlewares)) {
      return handler();
    } else {
      auto& m = std::get<I>(middlewares_);

      if constexpr (requires { m.around(ctx, std::declval<next_handler>()); })
        return m.around(ctx, next_handler([this, &ctx, &handler]() { return around_from<I + 1>(ctx, handler); }));
      else
        return around_from<I + 1>(ctx, handler);
    }
  }

  template<typename M>
  static bool call_before(M& m, context_type& ctx)
  {
    if constexpr (requires { { m.before(ctx) } -> std::convertible_to<bool>; }) {
      return static_cast<bool>(m.before(ctx));
    } else if constexpr (requires { m.before(ctx); }) {
      m.before(ctx);
      return true;
    } else {
      return true;
    }
  }

  template<typename M>
  static void call_after(M& m, context_type& ctx)
  {
    if constexpr (requires { m.after(ctx); })
      m.after(ctx);
  }

  template<std::size_t... Is>
  bool before_impl(context_type& ctx, std::size_t& entered, std::index_sequence<Is...>)
  {
    // Short-circuits on the first middleware returning false
    return ((++entered, call_before(std::get<Is>(middlewares_), ctx)) && ...);
  }

  template<std::size_t... Is>
  void after_impl(context_type& ctx, std::size_t entered, std::index_sequence<Is...>)
  {
    constexpr std::size_t count = sizeof...(Middlewares);
    ((count - 1 - Is < entered ? call_after(std::get<count - 1 - Is>(middlewares_), ctx) : void()), ...);
  }

private:
  std::tuple<Middlewares...> middlewares_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_SERVER_MIDDLEWARE_HPP
