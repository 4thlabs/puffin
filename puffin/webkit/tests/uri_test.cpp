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

#include <puffin/webkit/uri/uri.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace puffin::webkit;

TEST_CASE("Url can be parsed or not", "[webkit][uri]")
{
  SECTION("Url is valid")
  {
    uri valid_uri("http://www.google.fr/path/to/12#toto?param=&params2=45");
    REQUIRE(valid_uri.scheme() == "http");
    REQUIRE(valid_uri.host() == "www.google.fr");
    REQUIRE(valid_uri.port() == 80);
    REQUIRE(valid_uri.path() == "/path/to/12");
    REQUIRE(valid_uri.fragment() == "toto");
    REQUIRE(valid_uri.query_string() == "param=&params2=45");
    REQUIRE(valid_uri.target() == "/path/to/12?param=&params2=45");
  }

  SECTION("Url without path")
  {
    uri valid_uri("http://www.google.fr");
    REQUIRE(valid_uri.scheme() == "http");
    REQUIRE(valid_uri.host() == "www.google.fr");
    REQUIRE(valid_uri.path() == "/");
  }

  SECTION("Port")
  {
    REQUIRE(uri("https://example.com/a").port() == 443);
    REQUIRE(uri("http://example.com:8080/a").port() == 8080);
    REQUIRE(uri("http://example.com:8080/a").str() == "http://example.com:8080/a");
  }

  SECTION("Path is valid")
  {
    uri valid_uri("/path/to/12#toto?param=&params2=45");
    REQUIRE(valid_uri.path() == "/path/to/12");
    REQUIRE(valid_uri.fragment() == "toto");
    REQUIRE(valid_uri.query_string() == "param=&params2=45");
  }

  SECTION("Url is invalid")
  {
    REQUIRE_THROWS_AS(uri("htt://www.google.fr/path/to/12#toto?param=123"), uri_parsing_error);
  }
}

TEST_CASE("Query strings are decoded", "[webkit][uri]")
{
  auto q = parse_query("a=1&b=hello%20world&a=2&flag");
  REQUIRE(q.count("a") == 2);
  REQUIRE(q.find("b")->second == "hello world");
  REQUIRE(q.find("flag")->second.empty());
}
