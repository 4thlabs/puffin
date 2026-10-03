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

#ifndef PUFFIN_WEBKIT_URI_HPP
#define PUFFIN_WEBKIT_URI_HPP

#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/uri/uri_parser.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {

using query_map = std::multimap<std::string, std::string, std::less<>>;

/**
 * @brief Parses a query string (a=1&b=2), names and values are percent-decoded
 */
inline query_map parse_query(std::string_view query)
{
  query_map params;

  while (!query.empty()) {
    auto amp = query.find('&');
    auto item = query.substr(0, amp);

    if (!item.empty()) {
      auto equal = item.find('=');
      auto name = item.substr(0, equal);
      auto value = equal == std::string_view::npos ? std::string_view() : item.substr(equal + 1);

      params.emplace(detail::percent_decode(name), detail::percent_decode(value));
    }

    if (amp == std::string_view::npos)
      break;

    query.remove_prefix(amp + 1);
  }

  return params;
}

/**
 * @brief An uri, parsed by the Parser policy
 */
template<typename Parser>
class basic_uri {
public:
  using parser_type = Parser;
  friend parser_type;

  basic_uri() = default;

  basic_uri(const std::string& url) { parse(url); }

  basic_uri(const std::string& host, const std::string& path) { parse(host + path); }

  const std::string& scheme() const { return scheme_; }
  const std::string& host() const { return host_; }

  /// The explicit port, or the default port of the scheme
  std::uint16_t port() const
  {
    if (port_)
      return *port_;

    return scheme_ == "https" ? 443 : 80;
  }

  bool has_explicit_port() const { return port_.has_value(); }

  std::string path() const { return path_.empty() ? std::string("/") : path_; }

  const std::string& fragment() const { return fragment_; }
  const std::string& query_string() const { return query_string_; }

  query_map query() const { return parse_query(query_string_); }

  /// The request target, as sent in the request line (path?query)
  std::string target() const { return path() + (query_string_.empty() ? "" : "?" + query_string_); }

  std::string str() const
  {
    return scheme_ + "://" + host_ + (port_ ? ":" + std::to_string(*port_) : "") + path_ +
           (fragment_.empty() ? "" : "#" + fragment_) + (query_string_.empty() ? "" : "?" + query_string_);
  }

protected:
  void parse(const std::string& url) { parser_type{}.parse(*this, url); }

private:
  std::string scheme_;
  std::string host_;
  std::optional<std::uint16_t> port_;
  std::string path_;
  std::string fragment_;
  std::string query_string_;
};

template<typename P>
inline std::ostream& operator<<(std::ostream& o, const basic_uri<P>& uri)
{
  return o << uri.str();
}

using uri = basic_uri<regex_parser>;

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_URI_HPP
