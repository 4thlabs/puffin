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

#ifndef PUFFIN_WEBKIT_SERVER_CONNECTION_HPP
#define PUFFIN_WEBKIT_SERVER_CONNECTION_HPP

#include <puffin/async.hpp>
#include <puffin/webkit/http/errors.hpp>
#include <puffin/webkit/http/parser.hpp>
#include <puffin/webkit/http/serializer.hpp>
#include <puffin/webkit/transport/concepts.hpp>
#include <puffin/webkit/transport/reader.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace puffin {
namespace webkit {

struct server_options {
  parser_limits limits;
  std::size_t read_buffer_size = 16 * 1024;
};

/**
 * @brief Decides if the connection stays open after a response, and sets its Connection header accordingly.
 *
 * The connection is kept alive when the request allows it and the handler did not send Connection: close.
 * HTTP/1.0 clients are told explicitly, since close is their default.
 */
inline bool apply_keep_alive(const request& req, response& res)
{
  const bool keep_alive = req.keep_alive() && !res.headers().has_token("Connection", "close");

  if (!keep_alive)
    res.headers().set("Connection", "close");
  else if (req.version() < http_1_1)
    res.headers().set("Connection", "keep-alive");

  return keep_alive;
}

/**
 * @brief Serves the requests of one connection until either side closes it.
 *
 * Requests are read, handed to the handler, answered in order. Handler is the server, called with
 * handle(request&, response&).
 */
template<Stream S, typename Handler>
class basic_connection {
public:
  basic_connection(S stream, const server_options& options, Handler& handler)
      : stream_(std::move(stream)), reader_(options.read_buffer_size), parser_(options.limits), handler_(handler)
  {}

  /// Transport errors end the connection silently
  async::async<void> run()
  {
    try {
      while (co_await serve_one()) {
      }
    } catch (...) {
    }

    stream_.close();
  }

private:
  /// A response, and how to send it
  struct reply {
    response res;
    bool head = false;       ///< Answers a HEAD request: headers only
    bool keep_alive = false; ///< The connection stays open afterwards
  };

  /// Reads, handles and answers one request. Returns false once the connection must be closed.
  async::async<bool> serve_one()
  {
    const read_status status = co_await reader_.read(stream_, parser_);

    if (status == read_status::end_of_stream)
      co_return false;

    reply r;

    if (status == read_status::error)
      r = reject(parser_.error());
    else
      r = co_await respond(parser_.release());

    const std::string wire = serialize(r.res, {.omit_body = r.head});

    co_await stream_.write(std::span<const char>(wire));
    co_return r.keep_alive;
  }

  async::async<reply> respond(request req)
  {
    reply r{.head = req.method() == "HEAD"};

    co_await handler_.handle(req, r.res);
    r.keep_alive = apply_keep_alive(req, r.res);

    co_return r;
  }

  /// The answer to a request that can't be parsed, the connection is closed after it
  static reply reject(parse_error e)
  {
    reply r{.res = response(to_status(e))};
    r.res.headers().set("Connection", "close");
    return r;
  }

private:
  S stream_;
  message_reader reader_;
  request_parser parser_;
  Handler& handler_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_SERVER_CONNECTION_HPP
