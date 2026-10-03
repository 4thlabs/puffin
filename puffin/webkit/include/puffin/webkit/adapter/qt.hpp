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

#ifndef PUFFIN_WEBKIT_ADAPTER_QT_HPP
#define PUFFIN_WEBKIT_ADAPTER_QT_HPP

// Qt 6 transport for puffin::webkit (target puffin::webkit_qt). Streams, acceptors and connectors
// must be used from the thread their Qt objects live in, typically with puffin::async::qt::executor.

#include <puffin/async/async.hpp>
#include <puffin/async/from_callback.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <QHostAddress>
#include <QMetaObject>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace puffin {
namespace webkit {
namespace qt {

namespace detail {

/// Deletes QObjects from their event loop, safe while signals are being delivered
struct delete_later {
  void operator()(QObject* object) const
  {
    if (object)
      object->deleteLater();
  }
};

template<typename T>
using qobject_ptr = std::unique_ptr<T, delete_later>;

template<typename Sender, typename Signal>
struct signal_ref {
  Sender* sender;
  Signal signal;
};

/**
 * @brief Awaits the first emission among several signals, their arguments are ignored
 */
template<typename... Senders, typename... Signals>
auto first_of(signal_ref<Senders, Signals>... refs)
{
  return async::from_callback<>([refs...](auto callback) {
    struct state {
      std::vector<QMetaObject::Connection> connections;
      bool fired = false;
    };

    auto s = std::make_shared<state>();

    auto fire = [s, callback]() {
      if (s->fired)
        return;

      s->fired = true;

      for (auto& c : s->connections)
        QObject::disconnect(c);

      callback();
    };

    (s->connections.push_back(QObject::connect(refs.sender, refs.signal, refs.sender, [fire]() { fire(); })),
     ...);
  });
}

template<typename Sender, typename Signal>
signal_ref<Sender, Signal> on(Sender* sender, Signal signal)
{
  return {sender, signal};
}

} // namespace detail

/**
 * @brief A connected QTcpSocket, satisfies the Stream concept. Owns the socket.
 */
class tcp_stream {
public:
  explicit tcp_stream(QTcpSocket* socket)
      : socket_(socket)
  {}

  async::async<std::size_t> read_some(std::span<char> buffer)
  {
    for (;;) {
      if (socket_->bytesAvailable() > 0) {
        qint64 n = socket_->read(buffer.data(), static_cast<qint64>(buffer.size()));

        if (n < 0)
          throw std::runtime_error(socket_->errorString().toStdString());

        co_return static_cast<std::size_t>(n);
      }

      if (socket_->state() != QAbstractSocket::ConnectedState) {
        auto error = socket_->error();

        // Closed by the peer, or by us
        if (error == QAbstractSocket::RemoteHostClosedError || error == QAbstractSocket::UnknownSocketError)
          co_return 0;

        throw std::runtime_error(socket_->errorString().toStdString());
      }

      co_await detail::first_of(detail::on(socket_.get(), &QTcpSocket::readyRead),
                                detail::on(socket_.get(), &QTcpSocket::disconnected),
                                detail::on(socket_.get(), &QTcpSocket::errorOccurred));
    }
  }

  async::async<std::size_t> write(std::span<const char> data)
  {
    if (socket_->write(data.data(), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size()))
      throw std::runtime_error(socket_->errorString().toStdString());

    while (socket_->bytesToWrite() > 0) {
      if (socket_->state() != QAbstractSocket::ConnectedState)
        throw std::runtime_error("Connection closed while writing: " + socket_->errorString().toStdString());

      co_await detail::first_of(detail::on(socket_.get(), &QTcpSocket::bytesWritten),
                                detail::on(socket_.get(), &QTcpSocket::disconnected),
                                detail::on(socket_.get(), &QTcpSocket::errorOccurred));
    }

    co_return data.size();
  }

  void close()
  {
    if (socket_)
      socket_->abort();
  }

  QTcpSocket* socket() const noexcept { return socket_.get(); }

private:
  detail::qobject_ptr<QTcpSocket> socket_;
};

/**
 * @brief Listens with a QTcpServer, satisfies the Acceptor concept
 */
class tcp_acceptor {
public:
  /// Listens on the given port (0 for any free port) and address (all interfaces by default)
  explicit tcp_acceptor(std::uint16_t port = 0, const QHostAddress& address = QHostAddress::Any)
      : server_(new QTcpServer()), closed_notifier_(new QObject())
  {
    if (!server_->listen(address, port))
      throw std::runtime_error("Failed to listen: " + server_->errorString().toStdString());
  }

  async::async<tcp_stream> accept()
  {
    for (;;) {
      if (!server_->isListening())
        throw std::runtime_error("Acceptor closed");

      if (server_->hasPendingConnections())
        co_return tcp_stream(server_->nextPendingConnection());

      co_await detail::first_of(detail::on(server_.get(), &QTcpServer::newConnection),
                                detail::on(closed_notifier_.get(), &QObject::objectNameChanged));
    }
  }

  /// Stops listening, a pending accept() completes with an error
  void close()
  {
    server_->close();
    closed_notifier_->setObjectName(QStringLiteral("closed")); // Emits objectNameChanged, waking up accept()
  }

  bool is_open() const { return server_->isListening(); }

  std::uint16_t port() const { return server_->serverPort(); }

  QTcpServer* server() const noexcept { return server_.get(); }

private:
  detail::qobject_ptr<QTcpServer> server_;
  std::unique_ptr<QObject> closed_notifier_;
};

/**
 * @brief Connects QTcpSockets, satisfies the Connector concept
 */
class tcp_connector {
public:
  async::async<tcp_stream> connect(std::string_view host, std::uint16_t port)
  {
    detail::qobject_ptr<QTcpSocket> socket(new QTcpSocket());
    socket->connectToHost(QString::fromUtf8(host.data(), static_cast<qsizetype>(host.size())), port);

    if (socket->state() != QAbstractSocket::ConnectedState) {
      co_await detail::first_of(detail::on(socket.get(), &QTcpSocket::connected),
                                detail::on(socket.get(), &QTcpSocket::errorOccurred));
    }

    if (socket->state() != QAbstractSocket::ConnectedState)
      throw std::runtime_error("Failed to connect: " + socket->errorString().toStdString());

    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    co_return tcp_stream(socket.release());
  }
};

static_assert(Stream<tcp_stream>);
static_assert(Acceptor<tcp_acceptor>);
static_assert(Connector<tcp_connector>);

} // namespace qt
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_ADAPTER_QT_HPP
