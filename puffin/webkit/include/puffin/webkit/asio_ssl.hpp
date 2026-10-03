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

#ifndef PUFFIN_WEBKIT_ASIO_SSL_HPP
#define PUFFIN_WEBKIT_ASIO_SSL_HPP

// TLS transport for puffin::webkit over asio and OpenSSL (target puffin::webkit_asio_ssl)

#include <puffin/webkit/asio.hpp>

#if defined(PUFFIN_ASYNC_USE_BOOST_ASIO)
#include <boost/asio/ssl.hpp>
#else
#include <asio/ssl.hpp>
#endif

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {
namespace asio {

/**
 * @brief A TLS stream over TCP, satisfies the Stream concept.
 *
 * Server side streams (from tls_acceptor) do their handshake on first use, so a failed handshake
 * only ends its own connection.
 */
class tls_stream {
public:
  using stream_type = net::ssl::stream<net::ip::tcp::socket>;

  tls_stream(stream_type stream, bool handshake_pending)
      : stream_(std::move(stream)), handshake_pending_(handshake_pending)
  {}

  async::async<std::size_t> read_some(std::span<char> buffer)
  {
    co_await ensure_handshake();

    auto [ec, n] = co_await stream_.async_read_some(net::buffer(buffer.data(), buffer.size()), use_async_tuple);

    // A peer closing the TCP connection without close_notify is common, treated as end of stream
    if (ec == net::error::eof || ec == net::ssl::error::stream_truncated)
      co_return 0;

    if (ec)
      throw ::puffin::async::detail::net_system_error(ec);

    co_return n;
  }

  async::async<std::size_t> write(std::span<const char> data)
  {
    co_await ensure_handshake();
    co_return co_await net::async_write(stream_, net::buffer(data.data(), data.size()), use_async);
  }

  /// Closes the TCP connection, without waiting for the TLS close_notify exchange
  void close()
  {
    ::puffin::async::detail::net_error_code ec;
    stream_.lowest_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
    stream_.lowest_layer().close(ec);
  }

  stream_type& stream() noexcept { return stream_; }

private:
  async::async<void> ensure_handshake()
  {
    if (handshake_pending_) {
      handshake_pending_ = false;
      co_await stream_.async_handshake(net::ssl::stream_base::server, use_async);
    }
  }

  stream_type stream_;
  bool handshake_pending_;
};

/**
 * @brief Listens on a TCP endpoint and hands out TLS streams, satisfies the Acceptor concept.
 *        The SSL context (certificate, key) must outlive the acceptor and its streams.
 */
class tls_acceptor {
public:
  tls_acceptor(net::any_io_executor executor, const net::ip::tcp::endpoint& endpoint, net::ssl::context& context)
      : tcp_(std::move(executor), endpoint), context_(context)
  {}

  tls_acceptor(net::io_context& io_context, std::uint16_t port, net::ssl::context& context,
               const std::string& address = "0.0.0.0")
      : tcp_(io_context, port, address), context_(context)
  {}

  async::async<tls_stream> accept()
  {
    tcp_stream tcp = co_await tcp_.accept();
    co_return tls_stream(tls_stream::stream_type(std::move(tcp.socket()), context_), true);
  }

  void close() { tcp_.close(); }
  bool is_open() const { return tcp_.is_open(); }
  std::uint16_t port() const { return tcp_.port(); }

private:
  tcp_acceptor tcp_;
  net::ssl::context& context_;
};

/**
 * @brief Connects TLS streams, with SNI and host name verification, satisfies the Connector concept.
 *        The SSL context must outlive the connector and its streams.
 */
class tls_connector {
public:
  tls_connector(net::any_io_executor executor, net::ssl::context& context)
      : executor_(std::move(executor)), context_(context)
  {}

  tls_connector(net::io_context& io_context, net::ssl::context& context)
      : tls_connector(io_context.get_executor(), context)
  {}

  /// Disables certificate verification, for tests against self-signed servers only
  void verify_peer(bool verify) { verify_ = verify; }

  async::async<tls_stream> connect(std::string_view host, std::uint16_t port)
  {
    std::string h(host);
    tls_stream::stream_type stream(co_await tcp_connector::connect_socket(executor_, h, port), context_);

    // Server Name Indication
    if (!SSL_set_tlsext_host_name(stream.native_handle(), h.c_str())) {
      ::puffin::async::detail::net_error_code ec(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
      throw ::puffin::async::detail::net_system_error(ec);
    }

    if (verify_) {
      stream.set_verify_mode(net::ssl::verify_peer);
      stream.set_verify_callback(net::ssl::host_name_verification(h));
    } else {
      stream.set_verify_mode(net::ssl::verify_none);
    }

    co_await stream.async_handshake(net::ssl::stream_base::client, use_async);
    co_return tls_stream(std::move(stream), false);
  }

private:
  net::any_io_executor executor_;
  net::ssl::context& context_;
  bool verify_ = true;
};

static_assert(Stream<tls_stream>);
static_assert(Acceptor<tls_acceptor>);
static_assert(Connector<tls_connector>);

} // namespace asio
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_ASIO_SSL_HPP
