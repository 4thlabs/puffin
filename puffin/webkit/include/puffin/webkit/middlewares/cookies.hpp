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

#ifndef PUFFIN_WEBKIT_MIDDLEWARES_COOKIES_HPP
#define PUFFIN_WEBKIT_MIDDLEWARES_COOKIES_HPP

#include <puffin/webkit/http/cookie.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {
namespace middlewares {

/**
 * @brief Parses the request cookies and emits the cookies set by the handler.
 *
 * Adds to the context: ctx.cookies(), ctx.cookie(name) and ctx.set_cookie(cookie).
 */
class cookies {
public:
  class data_type {
  public:
    /// Cookies sent by the client
    const cookie_map& cookies() const { return received_; }

    std::optional<std::string_view> cookie(std::string_view name) const
    {
      auto it = received_.find(name);

      if (it == received_.end())
        return std::nullopt;

      return std::string_view(it->second);
    }

    /// Sends a cookie to the client with the response
    void set_cookie(webkit::cookie c) { sent_.push_back(std::move(c)); }

  private:
    friend class middlewares::cookies;

    cookie_map received_;
    std::vector<webkit::cookie> sent_;
  };

  template<typename Context>
  void before(Context& ctx)
  {
    auto& data = ctx.template data<cookies>();

    for (auto value : ctx.request().headers().get_all("Cookie")) {
      for (auto& [name, v] : parse_cookie_header(value))
        data.received_.emplace(name, std::move(v));
    }
  }

  template<typename Context>
  void after(Context& ctx)
  {
    auto& data = ctx.template data<cookies>();

    for (const auto& c : data.sent_)
      ctx.response().headers().add("Set-Cookie", c.str());
  }
};

} // namespace middlewares
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_MIDDLEWARES_COOKIES_HPP
