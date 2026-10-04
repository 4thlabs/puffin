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
#include <puffin/async/adapter/asio.hpp>
#include <puffin/webkit/adapter/asio.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/middlewares/session.hpp>
#include <puffin/webkit/server/server.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

using namespace std::chrono_literals;

TEST_CASE("Server and client over asio", "[webkit][asio]")
{
  ::asio::io_context io;
  pa::asio::executor executor{io};

  wk::asio::tcp_acceptor acceptor(io, 0, "127.0.0.1");

  wk::basic_server<wk::middlewares::session> server;

  server.get("/hello", [](auto& ctx) { ctx.response().body("hello", "text/plain"); });

  // Suspends on a timer before answering
  server.get("/slow/([0-9]+)", [](auto& ctx) -> pa::async<void> {
    co_await pa::sleep_for(10ms);
    ctx.response().body(std::string(ctx.param(0)));
  });

  server.post("/echo", [](auto& ctx) { ctx.response().body(ctx.request().body()); });

  server.get("/count", [](auto& ctx) {
    auto& count = ctx.session()["count"];
    count = std::to_string(count.empty() ? 1 : std::stoi(count) + 1);
    ctx.response().body(count);
  });

  pa::co_spawn(executor, server.listen(acceptor));

  std::thread io_thread([&] { io.run_for(20s); });

  struct results {
    std::string hello;
    std::string echo;
    std::vector<std::string> slow;
    int not_found = 0;
    bool kept_alive = false;
  };

  auto scenario = [&]() -> pa::async<results> {
    results r;
    wk::basic_client client(wk::asio::tcp_connector{io}, "127.0.0.1", acceptor.port());

    r.hello = (co_await client.get("/hello")).body();
    r.kept_alive = client.connected();
    r.echo = (co_await client.post("/echo", std::string(100000, 'x'))).body();
    r.not_found = (co_await client.get("/missing")).status_code();

    // Concurrent clients, each on its own connection
    auto slow = [&](int i) -> pa::async<std::string> {
      wk::basic_client c(wk::asio::tcp_connector{io}, "127.0.0.1", acceptor.port());
      co_return (co_await c.get("/slow/" + std::to_string(i))).body();
    };

    auto [a, b, c] = co_await pa::when_all(slow(1), slow(2), slow(3));
    r.slow = {a, b, c};

    co_return r;
  };

  results r = pa::sync_wait(executor, scenario());

  REQUIRE(r.hello == "hello");
  REQUIRE(r.kept_alive);
  REQUIRE(r.echo == std::string(100000, 'x'));
  REQUIRE(r.not_found == 404);
  REQUIRE(r.slow == std::vector<std::string>{"1", "2", "3"});

  // Session cookie round trip through a real connection
  auto counter = [&]() -> pa::async<std::vector<std::string>> {
    wk::basic_client client(wk::asio::tcp_connector{io}, "127.0.0.1", acceptor.port());
    std::vector<std::string> bodies;

    wk::response first = co_await client.get("/count");
    bodies.push_back(first.body());

    auto set_cookie = std::string(first.headers().get("Set-Cookie").value_or(""));
    wk::request next("GET", "/count");
    next.headers().set("Cookie", set_cookie.substr(0, set_cookie.find(';')));
    bodies.push_back((co_await client.request(std::move(next))).body());

    co_return bodies;
  };

  REQUIRE(pa::sync_wait(executor, counter()) == std::vector<std::string>{"1", "2"});

  // Closing the acceptor ends listen(), then every connection is done and io.run() returns
  pa::sync_wait(executor, [&]() -> pa::async<void> {
    acceptor.close();
    co_return;
  }());

  io_thread.join();
  REQUIRE_FALSE(acceptor.is_open());
}
