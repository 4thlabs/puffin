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

#ifndef PUFFIN_WEBKIT_HTTP_SERIALIZER_HPP
#define PUFFIN_WEBKIT_HTTP_SERIALIZER_HPP

#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <cstdio>
#include <string>

namespace puffin {
namespace webkit {

namespace detail {

inline bool is_chunked(const headers& h)
{
  return h.has_token("Transfer-Encoding", "chunked");
}

inline void append_headers(std::string& out, const headers& h)
{
  for (const auto& field : h) {
    out += field.name;
    out += ": ";
    out += field.value;
    out += "\r\n";
  }
}

/// Appends the body, framed according to the headers
inline void append_body(std::string& out, const headers& h, const std::string& body)
{
  if (!is_chunked(h)) {
    out += body;
    return;
  }

  if (!body.empty()) {
    char size[2 * sizeof(std::size_t) + 1];
    std::snprintf(size, sizeof(size), "%zx", body.size());
    out += size;
    out += "\r\n";
    out += body;
    out += "\r\n";
  }

  out += "0\r\n\r\n";
}

} // namespace detail

/**
 * @brief Serializes a request to its wire format.
 *
 * Content-Length is added when the body is not empty and no framing header was set.
 */
inline std::string serialize(const request& req)
{
  const auto& h = req.headers();
  std::string out;
  out.reserve(256 + req.body().size());

  out += req.method();
  out += ' ';
  out += req.target();
  out += ' ';
  out += req.version().str();
  out += "\r\n";

  detail::append_headers(out, h);

  if (!req.body().empty() && !h.contains("Content-Length") && !h.contains("Transfer-Encoding"))
    out += "Content-Length: " + std::to_string(req.body().size()) + "\r\n";

  out += "\r\n";
  detail::append_body(out, h, req.body());

  return out;
}

struct serialize_options {
  /// Writes the headers only, as for a response to HEAD: they still describe the body (Content-Length...)
  bool omit_body = false;
};

/**
 * @brief Serializes a response to its wire format.
 *
 * Content-Length is always added when no framing header was set, except for statuses that can't have a body.
 */
inline std::string serialize(const response& res, serialize_options options = {})
{
  const auto& h = res.headers();
  int code = res.status_code();
  bool bodyless = (code >= 100 && code < 200) || code == 204 || code == 304;

  std::string out;
  out.reserve(256 + res.body().size());

  out += res.version().str();
  out += ' ';
  out += std::to_string(code);
  out += ' ';
  out += res.reason();
  out += "\r\n";

  detail::append_headers(out, h);

  if (!bodyless && !h.contains("Content-Length") && !h.contains("Transfer-Encoding"))
    out += "Content-Length: " + std::to_string(res.body().size()) + "\r\n";

  out += "\r\n";

  if (!bodyless && !options.omit_body)
    detail::append_body(out, h, res.body());

  return out;
}

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_SERIALIZER_HPP
