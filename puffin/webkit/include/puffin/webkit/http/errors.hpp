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

#ifndef PUFFIN_WEBKIT_HTTP_ERRORS_HPP
#define PUFFIN_WEBKIT_HTTP_ERRORS_HPP

#include <puffin/webkit/http/parser.hpp>

#include <stdexcept>
#include <string>

namespace puffin {
namespace webkit {

inline const char* to_string(parse_error e) noexcept
{
  switch (e) {
    case parse_error::none: return "none";
    case parse_error::bad_start_line: return "bad start line";
    case parse_error::bad_version: return "bad version";
    case parse_error::bad_status_code: return "bad status code";
    case parse_error::bad_header: return "bad header";
    case parse_error::bad_content_length: return "bad content length";
    case parse_error::bad_transfer_encoding: return "bad transfer encoding";
    case parse_error::bad_chunk: return "bad chunk";
    case parse_error::header_too_large: return "header too large";
    case parse_error::body_too_large: return "body too large";
    case parse_error::unexpected_eof: return "unexpected end of stream";
  }

  return "unknown";
}

/**
 * @brief Thrown by the client when the peer sends an invalid HTTP message
 */
class protocol_error : public std::runtime_error {
public:
  explicit protocol_error(parse_error e)
      : std::runtime_error(std::string("HTTP protocol error: ") + to_string(e)), error_(e)
  {}

  parse_error error() const noexcept { return error_; }

private:
  parse_error error_;
};

/**
 * @brief Thrown by the client when the connection is closed before a response is received
 */
class connection_closed : public std::runtime_error {
public:
  connection_closed()
      : std::runtime_error("Connection closed before a response was received")
  {}
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_ERRORS_HPP
