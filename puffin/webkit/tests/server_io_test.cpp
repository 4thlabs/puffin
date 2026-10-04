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

#include "memory_transport.hpp"

#include <puffin/async.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/middlewares/cookies.hpp>
#include <puffin/webkit/middlewares/session.hpp>
#include <puffin/webkit/server/server.hpp>
#include <puffin/webkit/transport/reader.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

using namespace puffin::webkit;
using puffin::async::async;
using puffin::async::sync_wait;

namespace {

using server_type = basic_server<middlewares::cookies, middlewares::session>;

void setup(server_type& server)
{
  server.get("/hello", [](auto& ctx) { ctx.response().body("hello", "text/plain"); });

  server.get("/users/([0-9]+)", [](auto& ctx) -> async<void> {
    ctx.response().body("user " + std::string(ctx.param(0)));
    co_return;
  });

  server.post("/echo", [](auto& ctx) { ctx.response().body(ctx.request().body()); });

  server.get("/login", [](auto& ctx) { ctx.session()["user"] = "bob"; });

  server.get("/whoami", [](auto& ctx) {
    auto it = ctx.session().find("user");
    ctx.response().body(it == ctx.session().end() ? "nobody" : it->second);
  });

  server.get("/throw", [](auto&) { throw std::runtime_error("boom"); });

  server.get("/close", [](auto& ctx) { ctx.response().headers().set("Connection", "close"); });
}

/// Serves the given bytes on one connection, returns the bytes written back
std::string serve(server_type& server, std::string input, bool* closed = nullptr)
{
  test::memory_stream stream;
  stream.p->input = std::move(input);
  auto p = stream.p;

  sync_wait(server.serve(std::move(stream)));

  if (closed)
    *closed = p->closed;

  return p->output;
}

/// Parses every response of a connection
std::vector<response> responses(std::string_view wire)
{
  std::vector<response> result;
  response_parser parser;

  while (!wire.empty()) {
    auto r = parser.feed(wire);
    REQUIRE(r.status == parse_status::done);
    wire.remove_prefix(r.consumed);
    result.push_back(parser.release());
  }

  return result;
}

} // namespace

TEST_CASE("Message reader", "[webkit][reader]")
{
  test::memory_stream stream; // Reads 7 bytes at a time
  message_reader reader(16);
  request_parser parser;

  auto read = [&] { return sync_wait(reader.read(stream, parser)); };

  SECTION("A message split over several reads, followed by a pipelined one")
  {
    stream.p->input = "GET /first HTTP/1.1\r\nHost: a\r\n\r\nGET /second HTTP/1.1\r\nHost: a\r\n\r\n";

    REQUIRE(read() == read_status::done);
    REQUIRE(parser.release().target() == "/first");

    REQUIRE(read() == read_status::done);
    REQUIRE(parser.release().target() == "/second");

    REQUIRE(read() == read_status::end_of_stream);
    REQUIRE(parser.idle());
  }

  SECTION("End of stream in the middle of a message leaves the parser as is")
  {
    stream.p->input = "GET /first HTTP/1.1\r\n";

    REQUIRE(read() == read_status::end_of_stream);
    REQUIRE_FALSE(parser.idle());
  }

  SECTION("Invalid message")
  {
    stream.p->input = "NOT HTTP\r\n\r\n";

    REQUIRE(read() == read_status::error);
  }

  SECTION("clear() drops the bytes kept for the next message")
  {
    stream.p->input = "GET /first HTTP/1.1\r\n\r\nGET /second HTTP/1.1\r\n\r\n";

    REQUIRE(read() == read_status::done);
    parser.reset();
    reader.clear();
    stream.p->input.clear();

    REQUIRE(read() == read_status::end_of_stream);
    REQUIRE(parser.idle());
  }
}

TEST_CASE("Keep-alive policy", "[webkit][server]")
{
  auto apply = [](request req, response& res) { return apply_keep_alive(req, res); };

  SECTION("HTTP/1.1 is kept alive by default")
  {
    response res;
    REQUIRE(apply(request("GET", "/"), res));
    REQUIRE_FALSE(res.headers().contains("Connection"));
  }

  SECTION("Connection: close from the client")
  {
    request req("GET", "/");
    req.headers().set("Connection", "close");
    response res;

    REQUIRE_FALSE(apply(std::move(req), res));
    REQUIRE(res.headers().get("Connection") == "close");
  }

  SECTION("Connection: close from the handler")
  {
    response res;
    res.headers().set("Connection", "close");

    REQUIRE_FALSE(apply(request("GET", "/"), res));
  }

  SECTION("HTTP/1.0 is closed unless it asks for keep-alive, then it is told explicitly")
  {
    request req("GET", "/");
    req.version(http_1_0);
    response closed;
    REQUIRE_FALSE(apply(req, closed));
    REQUIRE(closed.headers().get("Connection") == "close");

    req.headers().set("Connection", "keep-alive");
    response kept;
    REQUIRE(apply(req, kept));
    REQUIRE(kept.headers().get("Connection") == "keep-alive");
  }
}

TEST_CASE("Server over a stream", "[webkit][server]")
{
  server_type server;
  setup(server);

  SECTION("Routes, parameters and pipelining on a kept alive connection")
  {
    bool closed = false;
    auto out = responses(serve(server,
                               "GET /hello HTTP/1.1\r\nHost: x\r\n\r\n"
                               "GET /users/42 HTTP/1.1\r\nHost: x\r\n\r\n"
                               "POST /echo HTTP/1.1\r\nContent-Length: 4\r\n\r\nping",
                               &closed));

    REQUIRE(out.size() == 3);
    REQUIRE(out[0].status_code() == 200);
    REQUIRE(out[0].body() == "hello");
    REQUIRE(out[0].headers().get("Content-Type") == "text/plain");
    REQUIRE(out[1].body() == "user 42");
    REQUIRE(out[2].body() == "ping");
    REQUIRE(closed); // End of input closes the connection
  }

  SECTION("404 and 405")
  {
    auto out = responses(serve(server, "GET /nope HTTP/1.1\r\n\r\nDELETE /hello HTTP/1.1\r\n\r\n"));

    REQUIRE(out[0].status_code() == 404);
    REQUIRE(out[1].status_code() == 405);
    REQUIRE(out[1].headers().get("Allow") == "GET");
  }

  SECTION("HEAD has the headers of GET, without body")
  {
    auto wire = serve(server, "HEAD /hello HTTP/1.1\r\n\r\n");
    REQUIRE(wire.find("Content-Length: 5\r\n") != std::string::npos);
    REQUIRE(wire.ends_with("\r\n\r\n"));
  }

  SECTION("Handler exception is a 500")
  {
    auto out = responses(serve(server, "GET /throw HTTP/1.1\r\n\r\nGET /hello HTTP/1.1\r\n\r\n"));
    REQUIRE(out[0].status_code() == 500);
    REQUIRE(out[1].status_code() == 200);
  }

  SECTION("Invalid request is a 400 and closes the connection")
  {
    auto out = responses(serve(server, "GARBAGE\r\n\r\nGET /hello HTTP/1.1\r\n\r\n"));
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].status_code() == 400);
    REQUIRE(out[0].headers().get("Connection") == "close");
  }

  SECTION("Connection: close is honored on both sides")
  {
    auto out = responses(serve(server, "GET /hello HTTP/1.1\r\nConnection: close\r\n\r\nGET /hello HTTP/1.1\r\n\r\n"));
    REQUIRE(out.size() == 1);

    out = responses(serve(server, "GET /close HTTP/1.1\r\n\r\nGET /hello HTTP/1.1\r\n\r\n"));
    REQUIRE(out.size() == 1);
  }

  SECTION("HTTP/1.0 keep-alive")
  {
    auto out = responses(serve(server, "GET /hello HTTP/1.0\r\nConnection: keep-alive\r\n\r\nGET /hello HTTP/1.0\r\n\r\n"));
    REQUIRE(out.size() == 2);
    REQUIRE(out[0].headers().get("Connection") == "keep-alive");
    REQUIRE(out[1].headers().get("Connection") == "close");
  }

  SECTION("Session middleware round trip")
  {
    auto login = responses(serve(server, "GET /login HTTP/1.1\r\n\r\n"));
    auto set_cookie = std::string(*login[0].headers().get("Set-Cookie"));
    auto cookie = set_cookie.substr(0, set_cookie.find(';'));

    auto out = responses(serve(server, "GET /whoami HTTP/1.1\r\nCookie: " + cookie + "\r\n\r\n"));
    REQUIRE(out[0].body() == "bob");
  }
}

TEST_CASE("Client over a stream", "[webkit][client]")
{
  test::memory_connector connector;

  SECTION("Request, keep-alive and chunked response")
  {
    auto p = connector.add("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi"
                           "HTTP/1.1 201 Created\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n");

    basic_client client(connector, "example.com", 8080);

    response first = sync_wait(client.get("/a"));
    REQUIRE(first.body() == "hi");
    REQUIRE(client.connected());

    response second = sync_wait(client.post("/b", "data", "text/plain"));
    REQUIRE(second.status_code() == 201);
    REQUIRE(second.body() == "abc");
    REQUIRE(*connector.connections == 1);

    REQUIRE(p->output == "GET /a HTTP/1.1\r\nHost: example.com:8080\r\n\r\n"
                         "POST /b HTTP/1.1\r\nContent-Type: text/plain\r\nHost: example.com:8080\r\n"
                         "Content-Length: 4\r\n\r\ndata");
  }

  SECTION("Interim responses are skipped")
  {
    connector.add("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    basic_client client(connector, "example.com", 80);

    REQUIRE(sync_wait(client.get("/")).status_code() == 200);
  }

  SECTION("Response delimited by the end of the connection")
  {
    connector.add("HTTP/1.0 200 OK\r\n\r\nuntil close");
    basic_client client(connector, "example.com", 80);

    REQUIRE(sync_wait(client.get("/")).body() == "until close");
    REQUIRE_FALSE(client.connected());
  }

  SECTION("Idempotent request retried once when the kept alive connection was closed")
  {
    connector.add("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\na");
    connector.add("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nb");
    basic_client client(connector, "example.com", 80);

    REQUIRE(sync_wait(client.get("/")).body() == "a");
    REQUIRE(sync_wait(client.get("/")).body() == "b");
    REQUIRE(*connector.connections == 2);
  }

  SECTION("Non idempotent request is not retried")
  {
    connector.add("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\na");
    connector.add("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nb");
    basic_client client(connector, "example.com", 80);

    sync_wait(client.get("/"));
    REQUIRE_THROWS_AS(sync_wait(client.post("/", "x")), connection_closed);
  }

  SECTION("Invalid response")
  {
    connector.add("NOT HTTP\r\n\r\n");
    basic_client client(connector, "example.com", 80);

    REQUIRE_THROWS_AS(sync_wait(client.get("/")), protocol_error);
    REQUIRE_FALSE(client.connected());
  }

  SECTION("Client and server through the memory transport")
  {
    server_type server;
    setup(server);

    std::string request_bytes;
    {
      // Capture what the client sends
      test::memory_connector capture;
      auto p = capture.add("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
      basic_client client(capture, "example.com", 80);
      sync_wait(client.get("/users/7"));
      request_bytes = p->output;
    }

    // Feed it to the server, and its answer back to a client
    connector.add(serve(server, request_bytes));
    basic_client client(connector, "example.com", 80);
    REQUIRE(sync_wait(client.get("/users/7")).body() == "user 7");
  }
}
