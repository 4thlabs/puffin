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

#ifndef PUFFIN_WEBKIT_CLIENT_CLIENT_HPP
#define PUFFIN_WEBKIT_CLIENT_CLIENT_HPP

#include <puffin/async.hpp>
#include <puffin/webkit/client/interceptor.hpp>
#include <puffin/webkit/http/errors.hpp>
#include <puffin/webkit/http/method.hpp>
#include <puffin/webkit/http/parser.hpp>
#include <puffin/webkit/http/serializer.hpp>
#include <puffin/webkit/transport/concepts.hpp>
#include <puffin/webkit/transport/reader.hpp>
#include <puffin/webkit/uri/uri.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace puffin {
namespace webkit {

struct client_options {
  parser_limits limits;
  std::size_t read_buffer_size = 16 * 1024;

  /// Keeps the connection open between requests
  bool keep_alive = true;
};

/**
 * @brief HTTP/1.1 client for one host, written with coroutines over any Connector (see the asio
 *        and Qt adapters).
 *
 *   basic_client client(asio::tcp_connector { io_context }, "example.com", 80);
 *   response res = co_await client.get("/index.html");
 *
 * The connection is opened on the first request and kept alive. A request failing on a reused
 * connection (closed by the server meanwhile) is retried once on a new one when idempotent.
 * One request at a time: use one client per concurrent task.
 *
 * Interceptors run around each request (see interceptor_chain), for instance:
 *
 *   basic_client<asio::tcp_connector, interceptors::bearer, interceptors::retry> client(connector, "example.com", 80);
 */
template<Connector C, typename... Interceptors>
class basic_client {
public:
  using connector_type = C;
  using stream_type = connected_stream_t<C>;

  basic_client(C connector, std::string host, std::uint16_t port, client_options options = {})
      : connector_(std::move(connector)), host_(std::move(host)), port_(port), options_(std::move(options))
  {}

  basic_client(C connector, std::string host, std::uint16_t port, client_options options, Interceptors... interceptors)
    requires(sizeof...(Interceptors) > 0)
      : connector_(std::move(connector)), host_(std::move(host)), port_(port), options_(std::move(options)),
        chain_(std::move(interceptors)...)
  {}

  /// Host and port are taken from the uri
  template<typename P>
  basic_client(C connector, const basic_uri<P>& base, client_options options = {})
      : basic_client(std::move(connector), base.host(), base.port(), std::move(options))
  {}

  basic_client(const basic_client&) = delete;
  basic_client& operator=(const basic_client&) = delete;

  ~basic_client() { close(); }

  /// Sends a request through the interceptors and returns its response
  async::async<response> request(webkit::request req)
  {
    if constexpr (sizeof...(Interceptors) == 0)
      return transmit(std::move(req));
    else
      return intercept(std::move(req));
  }

  /// Access to an interceptor instance, to configure it
  template<typename I>
  I& interceptor()
  {
    return chain_.template get<I>();
  }

  async::async<response> get(std::string target) { return request(webkit::request("GET", std::move(target))); }

  async::async<response> del(std::string target) { return request(webkit::request("DELETE", std::move(target))); }

  async::async<response> post(std::string target, std::string body,
                              std::string content_type = "application/octet-stream")
  {
    return request(with_body("POST", std::move(target), std::move(body), std::move(content_type)));
  }

  async::async<response> put(std::string target, std::string body,
                             std::string content_type = "application/octet-stream")
  {
    return request(with_body("PUT", std::move(target), std::move(body), std::move(content_type)));
  }

  /// True if a connection is currently open
  bool connected() const { return stream_.has_value(); }

  /// Closes the connection, the next request opens a new one
  void close()
  {
    if (stream_) {
      stream_->close();
      stream_.reset();
    }

    reader_.clear();
  }

  const std::string& host() const { return host_; }
  std::uint16_t port() const { return port_; }

private:
  /// Marks the client busy for the duration of a request
  struct busy_guard {
    explicit busy_guard(bool& busy)
        : busy_(busy)
    {
      if (busy_)
        throw std::logic_error("basic_client handles one request at a time");

      busy_ = true;
    }

    ~busy_guard() { busy_ = false; }

    busy_guard(const busy_guard&) = delete;
    busy_guard& operator=(const busy_guard&) = delete;

  private:
    bool& busy_;
  };

  async::async<response> intercept(webkit::request req)
  {
    // A copy for each attempt: an interceptor may send the request again
    auto send = [this](webkit::request& r) { return transmit(r); };
    co_return co_await chain_.send(req, send);
  }

  /// Sends a request on the connection. Host and Connection headers are filled if missing.
  async::async<response> transmit(webkit::request req)
  {
    busy_guard guard(busy_);
    complete_headers(req);

    const std::string wire = serialize(req);
    const bool head = req.method() == "HEAD";
    const bool reused = stream_.has_value();

    if (!stream_)
      stream_.emplace(co_await connector_.connect(host_, port_));

    auto first = co_await async::try_await(exchange(wire, head));

    if (first)
      co_return std::move(*first.value);

    close();

    if (!reused || !idempotent(req.method()))
      first.rethrow();

    // The server may have closed the kept alive connection meanwhile, retrying on a new one
    stream_.emplace(co_await connector_.connect(host_, port_));

    try {
      co_return co_await exchange(wire, head);
    } catch (...) {
      close();
      throw;
    }
  }

  void complete_headers(webkit::request& req) const
  {
    if (!req.headers().contains("Host"))
      req.headers().set("Host", port_ == 80 || port_ == 443 ? host_ : host_ + ":" + std::to_string(port_));

    if (!options_.keep_alive && !req.headers().contains("Connection"))
      req.headers().set("Connection", "close");
  }

  /// Writes a serialized request on the open connection and reads its response
  async::async<response> exchange(const std::string& wire, bool head)
  {
    co_await stream_->write(std::span<const char>(wire));

    response_parser parser(options_.limits);
    const bool closed = co_await read_final_response(parser, head);
    response res = parser.release();

    if (closed || !res.keep_alive() || !options_.keep_alive)
      close();

    co_return res;
  }

  /**
   * @brief Reads responses until a final one, interim responses (100 Continue...) are skipped, 101 is final.
   * @return true if the server closed the connection
   */
  async::async<bool> read_final_response(response_parser& parser, bool head)
  {
    for (;;) {
      parser.skip_body(head);
      const bool closed = co_await reader_.read(*stream_, parser) == read_status::end_of_stream;

      if (closed) {
        if (parser.idle())
          throw connection_closed();

        parser.finish();
      }

      if (parser.status() == parse_status::error)
        throw protocol_error(parser.error());

      if (closed || !is_interim(parser.message().status_code()))
        co_return closed;

      parser.reset();
    }
  }

  static bool is_interim(int code) { return code >= 100 && code < 200 && code != 101; }

  static webkit::request with_body(std::string method, std::string target, std::string body, std::string content_type)
  {
    webkit::request req(std::move(method), std::move(target));
    req.headers().set("Content-Type", std::move(content_type));
    req.body(std::move(body));
    return req;
  }

private:
  C connector_;
  std::string host_;
  std::uint16_t port_;
  client_options options_;

  std::optional<stream_type> stream_;
  message_reader reader_{options_.read_buffer_size};
  bool busy_ = false;
  interceptor_chain<Interceptors...> chain_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_CLIENT_CLIENT_HPP
