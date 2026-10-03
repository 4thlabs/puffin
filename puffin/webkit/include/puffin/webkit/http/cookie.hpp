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

#ifndef PUFFIN_WEBKIT_HTTP_COOKIE_HPP
#define PUFFIN_WEBKIT_HTTP_COOKIE_HPP

#include <puffin/webkit/detail/string.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {

/**
 * @brief A cookie sent by the server through a Set-Cookie header
 */
struct cookie {
  enum class same_site_policy { strict, lax, none };

  cookie() = default;

  cookie(std::string name, std::string value)
      : name(std::move(name)), value(std::move(value))
  {}

  std::string name;
  std::string value;

  std::string path = "/";
  std::optional<std::string> domain;
  std::optional<std::chrono::seconds> max_age;
  std::optional<same_site_policy> same_site;
  bool secure = false;
  bool http_only = false;

  /// Value of the Set-Cookie header for this cookie
  std::string str() const
  {
    std::string s = name + "=" + value;

    if (!path.empty())
      s += "; Path=" + path;

    if (domain)
      s += "; Domain=" + *domain;

    if (max_age)
      s += "; Max-Age=" + std::to_string(max_age->count());

    if (same_site) {
      switch (*same_site) {
        case same_site_policy::strict: s += "; SameSite=Strict"; break;
        case same_site_policy::lax: s += "; SameSite=Lax"; break;
        case same_site_policy::none: s += "; SameSite=None"; break;
      }
    }

    if (secure)
      s += "; Secure";

    if (http_only)
      s += "; HttpOnly";

    return s;
  }
};

using cookie_map = std::map<std::string, std::string, std::less<>>;

/**
 * @brief Parses the value of a Cookie request header (name1=value1; name2=value2)
 *
 * When a name appears several times, the first occurrence is kept (the most specific path, per RFC 6265).
 */
inline cookie_map parse_cookie_header(std::string_view value)
{
  cookie_map cookies;

  while (!value.empty()) {
    auto semicolon = value.find(';');
    auto pair = detail::trim(value.substr(0, semicolon));
    auto equal = pair.find('=');

    if (equal != std::string_view::npos && equal > 0) {
      auto name = detail::trim(pair.substr(0, equal));
      auto val = detail::trim(pair.substr(equal + 1));

      if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
        val = val.substr(1, val.size() - 2);

      cookies.emplace(std::string(name), std::string(val));
    }

    if (semicolon == std::string_view::npos)
      break;

    value.remove_prefix(semicolon + 1);
  }

  return cookies;
}

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_COOKIE_HPP
