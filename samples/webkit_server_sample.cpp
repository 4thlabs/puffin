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
#include <puffin/webkit.hpp>
#include <puffin/webkit/asio.hpp>

#include <asio/signal_set.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

using namespace std::chrono_literals;

// Counts the visits of each client in its session cookie:
//   curl -c jar -b jar http://127.0.0.1:8080/visits
int main(int argc, char** argv)
{
  std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::atoi(argv[1])) : 8080;

  ::asio::io_context io;
  pa::asio::executor executor{io};

  wk::basic_server<wk::middlewares::cookies, wk::middlewares::session> server;

  server.get("/", [](auto& ctx) { ctx.response().body("Hello from puffin\n", "text/plain"); });

  server.get("/visits", [](auto& ctx) {
    auto& visits = ctx.session()["visits"];
    visits = std::to_string(visits.empty() ? 1 : std::stoi(visits) + 1);
    ctx.response().body("Visits: " + visits + "\n", "text/plain");
  });

  server.get("/wait/([0-9]+)", [executor](auto& ctx) -> pa::async<void> {
    co_await pa::asio::sleep_for(executor, std::chrono::milliseconds(std::stoi(std::string(ctx.param(0)))));
    ctx.response().body("Waited\n", "text/plain");
  });

  wk::asio::tcp_acceptor acceptor(io, port);
  std::cout << "Listening on http://0.0.0.0:" << acceptor.port() << std::endl;

  pa::co_spawn(executor, server.listen(acceptor), [](std::exception_ptr e) {
    if (e)
      std::cerr << "Server stopped with an error" << std::endl;
  });

  // Ctrl+C closes the acceptor, run() returns once the connections are done
  ::asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&](auto, int) { acceptor.close(); });

  io.run();
  return 0;
}
