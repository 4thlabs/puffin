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

#include <puffin/async.hpp>
#include <puffin/async/qt.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/qt.hpp>
#include <puffin/webkit/server/server.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTimer>

#include <chrono>
#include <exception>
#include <string>
#include <vector>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

using namespace std::chrono_literals;

namespace {

QCoreApplication& application()
{
  // Shared with other Qt tests of the binary, only one application object may exist
  if (auto* app = QCoreApplication::instance())
    return *app;

  static int argc = 1;
  static char name[] = "main_test";
  static char* argv[] = {name, nullptr};
  static QCoreApplication app(argc, argv);
  return app;
}

} // namespace

TEST_CASE("Server and client over Qt", "[webkit][qt]")
{
  auto& app = application();

  wk::qt::tcp_acceptor acceptor(0, QHostAddress::LocalHost);

  wk::basic_server<> server;
  server.get("/hello", [](auto& ctx) { ctx.response().body("hello"); });

  server.get("/slow", [](auto& ctx) -> pa::async<void> {
    co_await pa::qt::sleep_for(10ms);
    ctx.response().body("slow");
  });

  server.post("/echo", [](auto& ctx) { ctx.response().body(ctx.request().body()); });

  pa::qt::executor executor;
  pa::co_spawn(executor, server.listen(acceptor));

  std::vector<std::string> bodies;
  bool kept_alive = false;
  std::exception_ptr error;

  pa::co_spawn(
      executor,
      [&]() -> pa::async<void> {
        {
          wk::basic_client client(wk::qt::tcp_connector{}, "127.0.0.1", acceptor.port());

          bodies.push_back((co_await client.get("/hello")).body());
          kept_alive = client.connected();
          bodies.push_back((co_await client.get("/slow")).body());
          bodies.push_back((co_await client.post("/echo", std::string(100000, 'q'))).body());
          bodies.push_back(std::to_string((co_await client.get("/missing")).status_code()));
        }

        acceptor.close();

        // Lets the server side connections see the end of stream
        co_await pa::qt::sleep_for(50ms);
      },
      [&](std::exception_ptr e) {
        error = e;
        QCoreApplication::quit();
      });

  QTimer::singleShot(20s, &app, [] { QCoreApplication::quit(); });
  app.exec();

  // Deletes the sockets released with deleteLater
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  REQUIRE_FALSE(error);
  REQUIRE(bodies == std::vector<std::string>{"hello", "slow", std::string(100000, 'q'), "404"});
  REQUIRE(kept_alive);
  REQUIRE_FALSE(acceptor.is_open());
}
