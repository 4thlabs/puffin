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

#ifndef PUFFIN_WEBKIT_HTTP_RESPONSE_HPP
#define PUFFIN_WEBKIT_HTTP_RESPONSE_HPP

#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/status.hpp>
#include <puffin/webkit/http/version.hpp>

#include <stdexcept>
#include <string>

namespace puffin {
namespace webkit {

/**
 * @brief An HTTP response message, used by both the client and the server
 */
class response {
public:
  response() = default;

  explicit response(webkit::status s, std::string body = {})
      : status_(static_cast<int>(s)), reason_(reason_phrase(s)), body_(std::move(body))
  {}

  int status_code() const { return status_; }

  const std::string& reason() const { return reason_; }

  /// Sets the status code, with its standard reason phrase
  void status(webkit::status s) { status(static_cast<int>(s)); }

  void status(int code) { status(code, std::string(reason_phrase(code))); }

  /// Throws std::invalid_argument if the code is not 3 digits or the reason contains CR, LF, NUL or other controls
  void status(int code, std::string reason)
  {
    if (code < 100 || code > 999 || !detail::is_field_value(reason))
      throw std::invalid_argument("invalid response status");

    status_ = code;
    reason_ = std::move(reason);
  }

  const webkit::version& version() const { return version_; }
  void version(webkit::version v) { version_ = v; }

  const webkit::headers& headers() const { return headers_; }
  webkit::headers& headers() { return headers_; }

  const std::string& body() const { return body_; }
  std::string& body() { return body_; }
  void body(std::string body) { body_ = std::move(body); }

  /// Sets the body and its Content-Type
  void body(std::string body, std::string content_type)
  {
    body_ = std::move(body);
    headers_.set("Content-Type", std::move(content_type));
  }

  /// True if the connection should be kept open after this response
  bool keep_alive() const
  {
    if (headers_.has_token("Connection", "close"))
      return false;

    return version_ >= http_1_1 || headers_.has_token("Connection", "keep-alive");
  }

private:
  int status_ = 200;
  std::string reason_ = "OK";
  webkit::version version_;
  webkit::headers headers_;
  std::string body_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_RESPONSE_HPP
