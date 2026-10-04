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
#include <catch2/catch_test_macros.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using puffin::async::any_executor;
using puffin::async::async;
using puffin::async::co_spawn;
using puffin::async::when_all;

namespace pa = puffin::async::asio;
using namespace std::chrono_literals;
using tcp = asio::ip::tcp;

namespace {

async<void> echo_once(tcp::acceptor& acceptor)
{
  tcp::socket socket = co_await acceptor.async_accept(pa::use_async);

  std::array<char, 64> buffer {};
  std::size_t n = co_await socket.async_read_some(asio::buffer(buffer), pa::use_async);
  co_await asio::async_write(socket, asio::buffer(buffer.data(), n), pa::use_async);
}

async<std::string> send(asio::io_context& context, tcp::endpoint endpoint, std::string message)
{
  tcp::socket socket(context);
  co_await socket.async_connect(endpoint, pa::use_async);
  co_await asio::async_write(socket, asio::buffer(message), pa::use_async);

  std::string reply(message.size(), '\0');
  auto [ec, n] = co_await asio::async_read(socket, asio::buffer(reply), pa::use_async_tuple);

  if (ec)
    co_return "error: " + ec.message();

  co_return reply.substr(0, n);
}

}

TEST_CASE("asio adapter", "[async][asio]")
{
  asio::io_context context;

  SECTION("co_spawn on an io_context")
  {
    int result = 0;

    co_spawn(
        pa::executor { context },
        []() -> async<int> { co_return 4; },
        [&](std::exception_ptr, int value) { result = value; });

    context.run();
    REQUIRE(result == 4);
  }

  SECTION("sleep_for on the asio executor")
  {
    auto start = std::chrono::steady_clock::now();
    bool done = false;

    co_spawn(pa::executor { context }, [&]() -> async<void> {
      co_await puffin::async::sleep_for(20ms);
      done = true;
    });

    context.run();
    REQUIRE(done);
    REQUIRE(std::chrono::steady_clock::now() - start >= 20ms);
  }

  SECTION("tcp echo")
  {
    tcp::acceptor acceptor(context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    std::string reply;

    co_spawn(pa::executor { context }, [&]() -> async<void> {
      auto [ignored, r] = co_await when_all(echo_once(acceptor), send(context, acceptor.local_endpoint(), "hello"));
      reply = r;
    });

    context.run();
    REQUIRE(reply == "hello");
  }

  SECTION("errors are thrown with use_async")
  {
    // Grab a free port and close it so that connecting fails
    tcp::endpoint endpoint;
    {
      tcp::acceptor acceptor(context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
      endpoint = acceptor.local_endpoint();
    }

    std::exception_ptr error;

    co_spawn(
        pa::executor { context },
        [&]() -> async<void> {
          tcp::socket socket(context);
          co_await socket.async_connect(endpoint, pa::use_async);
        },
        [&](std::exception_ptr ex) { error = ex; });

    context.run();
    REQUIRE(error);
    REQUIRE_THROWS_AS(std::rethrow_exception(error), asio::system_error);
  }

  SECTION("tasks switching between io_contexts")
  {
    // Owned executors (rvalues): the continuation is posted back across contexts
    asio::io_context other;
    auto guard = asio::make_work_guard(other);
    std::thread other_thread([&] { other.run(); });

    int count = 0;

    auto hop = [&]() -> async<int> {
      co_await puffin::async::schedule_on(pa::executor { other });
      co_return 1;
    };

    // context has no pending work while the tasks run on other, keep it alive until done
    auto context_guard = asio::make_work_guard(context);

    co_spawn(
        pa::executor { context },
        [&]() -> async<void> {
          std::vector<async<int>> tasks;
          for (int i = 0; i < 100; ++i)
            tasks.push_back(hop());

          for (int v : co_await when_all(std::move(tasks)))
            count += v;

          for (int i = 0; i < 100; ++i)
            count += co_await hop();
        },
        [&](std::exception_ptr) { context_guard.reset(); });

    context.run();
    guard.reset();
    other_thread.join();

    REQUIRE(count == 200);
  }

  SECTION("executor equality")
  {
    REQUIRE(any_executor(pa::executor { context }) == any_executor(pa::executor { context }));
  }
}
