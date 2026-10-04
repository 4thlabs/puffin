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

#ifndef PUFFIN_WEBKIT_INTERCEPTORS_BEARER_HPP
#define PUFFIN_WEBKIT_INTERCEPTORS_BEARER_HPP

#include <puffin/async/async.hpp>
#include <puffin/webkit/client/interceptor.hpp>
#include <puffin/webkit/http/status.hpp>

#include <functional>
#include <string>
#include <utility>

namespace puffin {
namespace webkit {
namespace interceptors {

/**
 * @brief Sends "Authorization: Bearer <token>" with every request. With a refresh function, a 401 refreshes the
 *        token and sends the request again, once.
 */
class bearer {
public:
  void token(std::string token) { token_ = std::move(token); }
  const std::string& token() const noexcept { return token_; }

  void on_refresh(std::function<async::async<std::string>()> refresh) { refresh_ = std::move(refresh); }

  async::async<response> operator()(request& req, next_request next)
  {
    authorize(req);
    response res = co_await next(req);

    if (res.status_code() != static_cast<int>(status::unauthorized) || !refresh_)
      co_return res;

    token_ = co_await refresh_();
    authorize(req);
    co_return co_await next(req);
  }

private:
  void authorize(request& req) const { req.headers().set("Authorization", "Bearer " + token_); }

  std::string token_;
  std::function<async::async<std::string>()> refresh_;
};

} // namespace interceptors
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_INTERCEPTORS_BEARER_HPP
