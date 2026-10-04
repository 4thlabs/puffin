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

#include <puffin/webkit/http/parser.hpp>
#include <puffin/webkit/http/serializer.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace puffin::webkit;

namespace {

/// Feeds the input byte per byte, to exercise every incremental path
template<typename Parser>
parse_status feed_bytewise(Parser& p, std::string_view input, std::size_t& consumed)
{
  consumed = 0;

  for (char c : input) {
    auto r = p.feed(std::string_view(&c, 1));
    consumed += r.consumed;

    if (r.status != parse_status::need_more)
      return r.status;
  }

  return parse_status::need_more;
}

} // namespace

TEST_CASE("Request parser", "[webkit][parser]")
{
  request_parser p;

  SECTION("Simple request")
  {
    std::string raw = "GET /index.html?x=1 HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n\r\n";
    auto r = p.feed(raw);

    REQUIRE(r.status == parse_status::done);
    REQUIRE(r.consumed == raw.size());
    REQUIRE(p.message().method() == "GET");
    REQUIRE(p.message().target() == "/index.html?x=1");
    REQUIRE(p.message().path() == "/index.html");
    REQUIRE(p.message().version() == http_1_1);
    REQUIRE(p.message().headers().get("host") == "example.com");
    REQUIRE(p.message().body().empty());
  }

  SECTION("Content-Length body, byte per byte")
  {
    std::string raw = "POST /items HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world";
    std::size_t consumed;

    REQUIRE(feed_bytewise(p, raw, consumed) == parse_status::done);
    REQUIRE(consumed == raw.size());
    REQUIRE(p.message().body() == "hello world");
  }

  SECTION("Chunked body with extensions and trailers")
  {
    std::string raw = "POST /up HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
                      "5;ext=1\r\nhello\r\n6\r\n world\r\n0\r\nX-Checksum: abc\r\n\r\n";
    std::size_t consumed;

    REQUIRE(feed_bytewise(p, raw, consumed) == parse_status::done);
    REQUIRE(consumed == raw.size());
    REQUIRE(p.message().body() == "hello world");
    REQUIRE(p.message().headers().get("X-Checksum") == "abc");
  }

  SECTION("Pipelined requests")
  {
    std::string first = "GET /a HTTP/1.1\r\n\r\n";
    std::string second = "GET /b HTTP/1.1\r\n\r\n";
    std::string raw = first + second;

    auto r = p.feed(raw);
    REQUIRE(r.status == parse_status::done);
    REQUIRE(r.consumed == first.size());
    REQUIRE(p.release().target() == "/a");

    r = p.feed(std::string_view(raw).substr(r.consumed));
    REQUIRE(r.status == parse_status::done);
    REQUIRE(p.message().target() == "/b");
  }

  SECTION("Leading empty lines are ignored")
  {
    REQUIRE(p.feed("\r\nGET / HTTP/1.1\r\n\r\n").status == parse_status::done);
  }

  SECTION("Errors")
  {
    auto fails_with = [](std::string_view raw, parse_error e) {
      request_parser parser;
      return parser.feed(raw).status == parse_status::error && parser.error() == e;
    };

    REQUIRE(fails_with("GET\r\n\r\n", parse_error::bad_start_line));
    REQUIRE(fails_with("GET / HTTX/1.1\r\n\r\n", parse_error::bad_version));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nBad Header: x\r\n\r\n", parse_error::bad_header));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nA: b\r\n folded\r\n\r\n", parse_error::bad_header));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nContent-Length: abc\r\n\r\n", parse_error::bad_content_length));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n",
                       parse_error::bad_content_length));
    REQUIRE(fails_with("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 3\r\n\r\n",
                       parse_error::bad_transfer_encoding));
    REQUIRE(fails_with("POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n", parse_error::bad_transfer_encoding));
    REQUIRE(fails_with("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", parse_error::bad_chunk));
    REQUIRE(fails_with("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nabc\r\n", parse_error::bad_chunk));
  }

  SECTION("Bare LF, lone CR and NUL are rejected in requests")
  {
    auto fails_with = [](std::string_view raw, parse_error e) {
      request_parser parser;
      return parser.feed(raw).status == parse_status::error && parser.error() == e;
    };

    REQUIRE(fails_with("GET / HTTP/1.1\nHost: a\r\n\r\n", parse_error::bad_line_ending));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nHost: a\n\r\n", parse_error::bad_line_ending));
    REQUIRE(fails_with("GET / HTTP/1.1\r\nX: a\rb\r\n\r\n", parse_error::bad_header));
    REQUIRE(fails_with(std::string_view("GET / HTTP/1.1\r\nX: a\0b\r\n\r\n", 26), parse_error::bad_header));
    REQUIRE(fails_with("GET /a\rb HTTP/1.1\r\n\r\n", parse_error::bad_start_line));
    REQUIRE(fails_with("GET /a\tb HTTP/1.1\r\n\r\n", parse_error::bad_start_line));
  }

  SECTION("Too long target is a 414")
  {
    request_parser parser;
    REQUIRE(parser.feed("GET /" + std::string(4096, 'a') + " HTTP/1.1\r\n\r\n").status == parse_status::error);
    REQUIRE(parser.error() == parse_error::uri_too_long);
  }

  SECTION("Limits")
  {
    request_parser small({.max_header_size = 64, .max_body_size = 4});

    REQUIRE(small.feed("GET /" + std::string(64, 'a') + " HTTP/1.1\r\n").status == parse_status::error);
    REQUIRE(small.error() == parse_error::header_too_large);

    small.reset();
    REQUIRE(small.feed("POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\n").status == parse_status::error);
    REQUIRE(small.error() == parse_error::body_too_large);
  }

  SECTION("Chunk size overflow can't bypass the body limit")
  {
    auto r = p.feed("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\nffffffffffffffff\r\n");
    REQUIRE(r.status == parse_status::error);
    REQUIRE(p.error() == parse_error::body_too_large);
  }

  SECTION("Chunk size line length is capped, even when complete")
  {
    auto r = p.feed("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n1;" + std::string(8192, 'x') + "\r\n");
    REQUIRE(r.status == parse_status::error);
    REQUIRE(p.error() == parse_error::bad_chunk);
  }

  SECTION("Unexpected end of stream")
  {
    p.feed("POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nab");
    REQUIRE(p.finish() == parse_status::error);
    REQUIRE(p.error() == parse_error::unexpected_eof);
  }
}

TEST_CASE("Response parser", "[webkit][parser]")
{
  response_parser p;

  SECTION("Content-Length body")
  {
    auto r = p.feed("HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\n\r\nnope");
    REQUIRE(r.status == parse_status::done);
    REQUIRE(p.message().status_code() == 404);
    REQUIRE(p.message().reason() == "Not Found");
    REQUIRE(p.message().body() == "nope");
  }

  SECTION("Control characters in the reason or a header are rejected")
  {
    REQUIRE(p.feed("HTTP/1.1 200 O\rK\r\n\r\n").status == parse_status::error);

    response_parser q;
    REQUIRE(q.feed(std::string_view("HTTP/1.1 200 OK\r\nX: a\0b\r\n\r\n", 27)).status == parse_status::error);
    REQUIRE(q.error() == parse_error::bad_header);
  }

  SECTION("Empty reason phrase")
  {
    REQUIRE(p.feed("HTTP/1.1 200\r\nContent-Length: 0\r\n\r\n").status == parse_status::done);
    REQUIRE(p.message().status_code() == 200);
    REQUIRE(p.message().reason().empty());
  }

  SECTION("Chunked body")
  {
    std::size_t consumed;
    std::string raw = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n";
    REQUIRE(feed_bytewise(p, raw, consumed) == parse_status::done);
    REQUIRE(p.message().body() == "abc");
  }

  SECTION("Body delimited by end of stream")
  {
    REQUIRE(p.feed("HTTP/1.0 200 OK\r\n\r\nsome ").status == parse_status::need_more);
    REQUIRE(p.feed("data").status == parse_status::need_more);
    REQUIRE(p.finish() == parse_status::done);
    REQUIRE(p.message().body() == "some data");
  }

  SECTION("Statuses without body")
  {
    REQUIRE(p.feed("HTTP/1.1 204 No Content\r\n\r\n").status == parse_status::done);

    p.reset();
    REQUIRE(p.feed("HTTP/1.1 304 Not Modified\r\nContent-Length: 10\r\n\r\n").status == parse_status::done);
  }

  SECTION("Response to HEAD")
  {
    p.skip_body();
    REQUIRE(p.feed("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n").status == parse_status::done);
    REQUIRE(p.message().body().empty());
  }

  SECTION("Bad status code")
  {
    REQUIRE(p.feed("HTTP/1.1 20x OK\r\n\r\n").status == parse_status::error);
    REQUIRE(p.error() == parse_error::bad_status_code);
  }
}

TEST_CASE("Serialized messages parse back", "[webkit][parser]")
{
  response res(status::created, "{\"id\":1}");
  res.headers().set("Content-Type", "application/json");

  response_parser p;
  REQUIRE(p.feed(serialize(res)).status == parse_status::done);
  REQUIRE(p.message().status_code() == 201);
  REQUIRE(p.message().headers().get("Content-Type") == "application/json");
  REQUIRE(p.message().body() == res.body());
}
