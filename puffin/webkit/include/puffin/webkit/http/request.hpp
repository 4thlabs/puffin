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

#ifndef PUFFIN_WEBKIT_HTTP_REQUEST_HPP
#define PUFFIN_WEBKIT_HTTP_REQUEST_HPP

#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/version.hpp>
#include <puffin/webkit/uri/uri.hpp>

#include <string>
#include <string_view>

namespace puffin {
namespace webkit {

/**
 * @brief An HTTP request message, used by both the client and the server
 */
class request {
public:
  request() = default;

  request(std::string method, std::string target)
      : method_(std::move(method)), target_(std::move(target))
  {}

  /// Builds a request for an absolute uri, the Host header is filled from it
  template<typename P>
  request(std::string method, const basic_uri<P>& uri)
      : method_(std::move(method)), target_(uri.target())
  {
    headers_.set("Host", uri.has_explicit_port() ? uri.host() + ":" + std::to_string(uri.port()) : uri.host());
  }

  const std::string& method() const { return method_; }
  void method(std::string method) { method_ = std::move(method); }

  /// The request target as sent on the request line (path?query)
  const std::string& target() const { return target_; }
  void target(std::string target) { target_ = std::move(target); }

  /// The path part of the target, without query
  std::string_view path() const
  {
    std::string_view t = target_;
    return t.substr(0, t.find_first_of("?#"));
  }

  /// The raw query string of the target, without '?'
  std::string_view query_string() const
  {
    std::string_view t = target_;
    auto q = t.find('?');

    if (q == std::string_view::npos)
      return {};

    t.remove_prefix(q + 1);
    return t.substr(0, t.find('#'));
  }

  query_map query() const { return parse_query(query_string()); }

  const webkit::version& version() const { return version_; }
  void version(webkit::version v) { version_ = v; }

  const webkit::headers& headers() const { return headers_; }
  webkit::headers& headers() { return headers_; }

  const std::string& body() const { return body_; }
  std::string& body() { return body_; }
  void body(std::string body) { body_ = std::move(body); }

  /// True if the connection should be kept open after this request
  bool keep_alive() const
  {
    if (headers_.has_token("Connection", "close"))
      return false;

    return version_ >= http_1_1 || headers_.has_token("Connection", "keep-alive");
  }

private:
  std::string method_ = "GET";
  std::string target_ = "/";
  webkit::version version_;
  webkit::headers headers_;
  std::string body_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_REQUEST_HPP
