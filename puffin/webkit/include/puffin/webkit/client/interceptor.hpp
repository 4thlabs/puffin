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

#ifndef PUFFIN_WEBKIT_CLIENT_INTERCEPTOR_HPP
#define PUFFIN_WEBKIT_CLIENT_INTERCEPTOR_HPP

#include <puffin/async/async.hpp>
#include <puffin/webkit/detail/continuation.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace puffin {
namespace webkit {

/// Continues a request from an interceptor: the next interceptor, or the transport
using next_request = detail::continuation<async::async<response>(request&)>;

/**
 * @brief The interceptors of a client. An interceptor is a class with
 *        async<response> operator()(request&, next_request), called in order around the sending of each request.
 *        It may change the request, answer it itself, or co_await next(req), possibly several times (retry).
 *        Each interceptor type appears once, get<I>() finds it by type.
 */
template<typename... Interceptors>
class interceptor_chain {
public:
  interceptor_chain() = default;

  explicit interceptor_chain(Interceptors... interceptors)
    requires(sizeof...(Interceptors) > 0)
      : interceptors_(std::move(interceptors)...)
  {}

  template<typename I>
  I& get()
  {
    return std::get<I>(interceptors_);
  }

  /**
   * @brief Sends a request through the interceptors, the last one continuing with send(request).
   *        Without interceptors the request is moved to send, otherwise each attempt sends a copy.
   */
  template<typename Send>
  async::async<response> run(request req, Send send)
  {
    if constexpr (sizeof...(Interceptors) == 0) {
      co_return co_await send(std::move(req));
    } else {
      co_return co_await send_from<0>(req, send); // send(request) takes a copy of req
    }
  }

private:
  template<std::size_t I, typename Send>
  async::async<response> send_from(request& req, Send& send)
  {
    if constexpr (I == sizeof...(Interceptors))
      return send(req);
    else
      return std::get<I>(interceptors_)(
          req, next_request([this, &send](request& r) { return send_from<I + 1>(r, send); }));
  }

private:
  std::tuple<Interceptors...> interceptors_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_CLIENT_INTERCEPTOR_HPP
