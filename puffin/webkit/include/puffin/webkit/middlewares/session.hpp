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

#ifndef PUFFIN_WEBKIT_MIDDLEWARES_SESSION_HPP
#define PUFFIN_WEBKIT_MIDDLEWARES_SESSION_HPP

#include <puffin/webkit/detail/crypto.hpp>
#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/http/cookie.hpp>

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {
namespace middlewares {

/**
 * @brief Stateless session, persisted client side in a cookie.
 *
 * Adds ctx.session() to the context, a map of strings restored from the request cookie and written back to the
 * response when modified. An emptied session expires the cookie.
 *
 * The cookie is signed with HMAC-SHA256 (payload.signature), a cookie with a missing or wrong signature is ignored
 * and the request starts with an empty session. The content is not encrypted: the client can read it, not change it.
 */
class session {
public:
  using session_map = std::map<std::string, std::string, std::less<>>;

  struct options {
    std::string cookie_name = "puffin_session";
    std::string path = "/";
    std::chrono::seconds max_age = std::chrono::hours(24);
    bool secure = false;
    bool http_only = true;

    /// Signing key. When empty a random key is generated, sessions are then lost on restart and not shared
    /// between server instances.
    std::string secret;

    /// Largest cookie value accepted or produced, a bigger session makes the response fail with a 500
    std::size_t max_cookie_size = 4096;
  };

  class data_type {
  public:
    session_map& session() { return session_; }
    const session_map& session() const { return session_; }

  private:
    friend class middlewares::session;

    session_map session_;
    session_map initial_;
  };

  session()
      : session(options{})
  {}

  explicit session(options opts)
      : options_(std::move(opts))
  {
    if (options_.secret.empty())
      options_.secret = random_secret();
  }

  template<typename Context>
  void before(Context& ctx)
  {
    auto& data = ctx.template data<session>();

    for (auto header : ctx.request().headers().get_all("Cookie")) {
      auto received = parse_cookie_header(header);
      auto it = received.find(options_.cookie_name);

      if (it != received.end()) {
        if (auto payload = verify(it->second))
          data.session_ = decode(*payload);

        break;
      }
    }

    data.initial_ = data.session_;
  }

  template<typename Context>
  void after(Context& ctx)
  {
    auto& data = ctx.template data<session>();

    if (data.session_ == data.initial_)
      return;

    webkit::cookie c;
    c.name = options_.cookie_name;
    c.value = data.session_.empty() ? std::string() : sign(encode(data.session_));

    if (c.value.size() > options_.max_cookie_size)
      throw std::length_error("session cookie too large");

    c.path = options_.path;
    c.max_age = data.session_.empty() ? std::chrono::seconds(0) : options_.max_age;
    c.secure = options_.secure;
    c.http_only = options_.http_only;

    ctx.response().headers().add("Set-Cookie", c.str());
  }

  /// Cookie value of the form key1=value1&key2=value2, keys and values are percent-encoded
  static std::string encode(const session_map& s)
  {
    std::string value;

    for (const auto& [k, v] : s) {
      if (!value.empty())
        value += '&';

      value += detail::percent_encode(k) + "=" + detail::percent_encode(v);
    }

    return value;
  }

  static session_map decode(std::string_view value)
  {
    session_map s;

    while (!value.empty()) {
      auto amp = value.find('&');
      auto item = value.substr(0, amp);
      auto equal = item.find('=');

      if (equal != std::string_view::npos)
        s.emplace(detail::percent_decode(item.substr(0, equal)), detail::percent_decode(item.substr(equal + 1)));

      if (amp == std::string_view::npos)
        break;

      value.remove_prefix(amp + 1);
    }

    return s;
  }

  /// payload.base64url(hmac)
  std::string sign(std::string_view payload) const
  {
    return std::string(payload) + "." + detail::base64url_encode(detail::hmac_sha256(options_.secret, payload));
  }

  /// The payload of a signed value, nullopt if the signature does not match
  std::optional<std::string_view> verify(std::string_view value) const
  {
    if (value.size() > options_.max_cookie_size)
      return std::nullopt;

    auto dot = value.rfind('.');

    if (dot == std::string_view::npos)
      return std::nullopt;

    auto payload = value.substr(0, dot);
    auto expected = detail::base64url_encode(detail::hmac_sha256(options_.secret, payload));

    if (!detail::constant_time_equal(value.substr(dot + 1), expected))
      return std::nullopt;

    return payload;
  }

private:
  static std::string random_secret()
  {
    std::random_device rd;
    std::string key(32, '\0');

    for (auto& c : key)
      c = static_cast<char>(rd() & 0xFF);

    return key;
  }

  options options_;
};

} // namespace middlewares
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_MIDDLEWARES_SESSION_HPP
