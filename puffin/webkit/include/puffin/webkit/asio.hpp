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

#ifndef PUFFIN_WEBKIT_ASIO_HPP
#define PUFFIN_WEBKIT_ASIO_HPP

// asio transport for puffin::webkit (target puffin::webkit_asio). Uses standalone asio by default,
// Boost.Asio when PUFFIN_ASYNC_USE_BOOST_ASIO is defined, like puffin::async_asio.

#include <puffin/async/adapter/asio.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#else
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#endif

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {
namespace asio {

namespace net = ::puffin::async::detail::net;

using ::puffin::async::asio::use_async;
using ::puffin::async::asio::use_async_tuple;

/**
 * @brief A connected TCP socket, satisfies the Stream concept
 */
class tcp_stream {
public:
  using socket_type = net::ip::tcp::socket;

  explicit tcp_stream(socket_type socket)
      : socket_(std::move(socket))
  {}

  async::async<std::size_t> read_some(std::span<char> buffer)
  {
    auto [ec, n] = co_await socket_.async_read_some(net::buffer(buffer.data(), buffer.size()), use_async_tuple);

    if (ec == net::error::eof)
      co_return 0;

    if (ec)
      throw ::puffin::async::detail::net_system_error(ec);

    co_return n;
  }

  async::async<std::size_t> write(std::span<const char> data)
  {
    co_return co_await net::async_write(socket_, net::buffer(data.data(), data.size()), use_async);
  }

  void close()
  {
    ::puffin::async::detail::net_error_code ec;
    socket_.shutdown(socket_type::shutdown_both, ec);
    socket_.close(ec);
  }

  socket_type& socket() noexcept { return socket_; }

private:
  socket_type socket_;
};

/**
 * @brief Listens on a TCP endpoint, satisfies the Acceptor concept
 */
class tcp_acceptor {
public:
  using acceptor_type = net::ip::tcp::acceptor;

  tcp_acceptor(net::any_io_executor executor, const net::ip::tcp::endpoint& endpoint)
      : acceptor_(executor, endpoint, /* reuse_address */ true)
  {}

  /// Listens on the given port (0 for any free port) of the given address (all interfaces by default)
  tcp_acceptor(net::io_context& context, std::uint16_t port, const std::string& address = "0.0.0.0")
      : tcp_acceptor(context.get_executor(), net::ip::tcp::endpoint(net::ip::make_address(address), port))
  {}

  async::async<tcp_stream> accept()
  {
    for (;;) {
      auto [ec, socket] = co_await acceptor_.async_accept(use_async_tuple);

      // The peer gave up before the connection was accepted, waiting for the next one
      if (ec == net::error::connection_aborted)
        continue;

      if (ec)
        throw ::puffin::async::detail::net_system_error(ec);

      socket.set_option(net::ip::tcp::no_delay(true));
      co_return tcp_stream(std::move(socket));
    }
  }

  void close()
  {
    ::puffin::async::detail::net_error_code ec;
    acceptor_.close(ec);
  }

  bool is_open() const { return acceptor_.is_open(); }

  /// The local port, useful when listening on port 0
  std::uint16_t port() const { return acceptor_.local_endpoint().port(); }

  acceptor_type& acceptor() noexcept { return acceptor_; }

private:
  acceptor_type acceptor_;
};

/**
 * @brief Resolves and connects to TCP endpoints, satisfies the Connector concept
 */
class tcp_connector {
public:
  explicit tcp_connector(net::any_io_executor executor)
      : executor_(std::move(executor))
  {}

  explicit tcp_connector(net::io_context& context)
      : executor_(context.get_executor())
  {}

  async::async<tcp_stream> connect(std::string_view host, std::uint16_t port)
  {
    co_return tcp_stream(co_await connect_socket(executor_, host, port));
  }

  /// Resolves host and connects a socket to the first reachable endpoint
  static async::async<net::ip::tcp::socket> connect_socket(net::any_io_executor executor, std::string_view host,
                                                           std::uint16_t port)
  {
    std::string h(host);
    net::ip::tcp::resolver resolver(executor);
    auto endpoints = co_await resolver.async_resolve(h, std::to_string(port), use_async);

    net::ip::tcp::socket socket(executor);
    co_await net::async_connect(socket, endpoints, use_async);
    socket.set_option(net::ip::tcp::no_delay(true));

    co_return std::move(socket);
  }

private:
  net::any_io_executor executor_;
};

static_assert(Stream<tcp_stream>);
static_assert(Acceptor<tcp_acceptor>);
static_assert(Connector<tcp_connector>);

} // namespace asio
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_ASIO_HPP
