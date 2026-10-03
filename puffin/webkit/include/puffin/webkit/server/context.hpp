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

#ifndef PUFFIN_WEBKIT_SERVER_CONTEXT_HPP
#define PUFFIN_WEBKIT_SERVER_CONTEXT_HPP

#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {

/**
 * @brief Per request data contributed by a middleware to the context.
 *
 * A middleware declaring a nested data_type extends the context with it, so its members are reachable directly
 * from the context (ctx.session(), ctx.cookies()...). Middlewares without data_type contribute an empty base.
 */
template<typename M>
struct middleware_data {};

template<typename M>
  requires requires { typename M::data_type; }
struct middleware_data<M> : M::data_type {};

/**
 * @brief The context handed to route handlers and middlewares for one request.
 *
 * It holds the request, the response being built, the route parameters, and the data of every middleware of
 * the server. Everything is resolved at compile time from the middleware list.
 */
template<typename... Middlewares>
class basic_context : public middleware_data<Middlewares>... {
public:
  basic_context(webkit::request& req, webkit::response& res)
      : request_(req), response_(res)
  {}

  basic_context(const basic_context&) = delete;
  basic_context& operator=(const basic_context&) = delete;

  webkit::request& request() { return request_; }
  const webkit::request& request() const { return request_; }

  webkit::response& response() { return response_; }
  const webkit::response& response() const { return response_; }

  /// Route parameters, the capture groups of the matched route, in order
  const std::vector<std::string>& params() const { return params_; }

  std::string_view param(std::size_t index) const
  {
    return index < params_.size() ? std::string_view(params_[index]) : std::string_view();
  }

  void params(std::vector<std::string> params) { params_ = std::move(params); }

  /// Explicit access to the data of a middleware, useful when two middlewares expose the same names
  template<typename M>
  middleware_data<M>& data()
  {
    return static_cast<middleware_data<M>&>(*this);
  }

  template<typename M>
  const middleware_data<M>& data() const
  {
    return static_cast<const middleware_data<M>&>(*this);
  }

private:
  webkit::request& request_;
  webkit::response& response_;
  std::vector<std::string> params_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_SERVER_CONTEXT_HPP
