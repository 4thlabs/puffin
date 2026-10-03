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

#include <puffin/webkit/http/cookie.hpp>
#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>
#include <puffin/webkit/http/serializer.hpp>
#include <puffin/webkit/detail/crypto.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace puffin::webkit;

TEST_CASE("Headers", "[webkit][http]")
{
  headers h;
  h.add("Content-Type", "text/plain");
  h.add("Set-Cookie", "a=1");
  h.add("set-cookie", "b=2");

  SECTION("Names are case insensitive")
  {
    REQUIRE(h.get("content-type") == "text/plain");
    REQUIRE(h.get_all("SET-COOKIE").size() == 2);
    REQUIRE_FALSE(h.contains("Content-Length"));
  }

  SECTION("set replaces every occurrence")
  {
    h.set("Set-Cookie", "c=3");
    REQUIRE(h.get_all("Set-Cookie").size() == 1);
    REQUIRE(h.get("Set-Cookie") == "c=3");
  }

  SECTION("Tokens")
  {
    h.add("Connection", "keep-alive, Upgrade");
    REQUIRE(h.has_token("connection", "upgrade"));
    REQUIRE_FALSE(h.has_token("connection", "close"));
  }
}

TEST_CASE("SHA-256 and HMAC-SHA256", "[webkit][http]")
{
  auto hex = [](const detail::sha256_digest& d) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (auto b : d) {
      out += digits[b >> 4];
      out += digits[b & 0x0F];
    }
    return out;
  };

  REQUIRE(hex(detail::sha256::hash("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  REQUIRE(hex(detail::sha256::hash("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  REQUIRE(hex(detail::sha256::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  REQUIRE(hex(detail::sha256::hash(std::string(1000000, 'a'))) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  // RFC 4231 test cases 2 and 6 (key longer than the block size)
  REQUIRE(hex(detail::hmac_sha256("Jefe", "what do ya want for nothing?")) ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  REQUIRE(hex(detail::hmac_sha256(std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First")) ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

  REQUIRE(detail::base64url_encode(std::string_view("\xfb\xff", 2)) == "-_8");
  REQUIRE(detail::base64url_encode(std::string_view("foobar")) == "Zm9vYmFy");
  REQUIRE(detail::base64url_encode(std::string_view("fooba")) == "Zm9vYmE");
  REQUIRE(detail::base64url_encode(std::string_view("f")) == "Zg");
}

TEST_CASE("Header injection is rejected", "[webkit][http]")
{
  headers h;

  REQUIRE_THROWS_AS(h.add("X", "a\r\nSet-Cookie: evil=1"), std::invalid_argument);
  REQUIRE_THROWS_AS(h.set("X", "a\nb"), std::invalid_argument);
  REQUIRE_THROWS_AS(h.add("X", std::string("a\0b", 3)), std::invalid_argument);
  REQUIRE_THROWS_AS(h.add("Bad Name", "a"), std::invalid_argument);
  REQUIRE_THROWS_AS(h.add("X:Y", "a"), std::invalid_argument);
  REQUIRE(h.empty());

  h.add("X", "tab\tand obs-text \xE9");
  REQUIRE(h.get("X") == "tab\tand obs-text \xE9");

  response res;
  REQUIRE_THROWS_AS(res.status(200, "OK\r\nX: y"), std::invalid_argument);
  REQUIRE_THROWS_AS(res.status(42, "Nope"), std::invalid_argument);

  REQUIRE_THROWS_AS(request("GET\r\n", "/"), std::invalid_argument);
  REQUIRE_THROWS_AS(request("GET", "/a b"), std::invalid_argument);
  REQUIRE_THROWS_AS(request("GET", "/a\r\nHost: evil"), std::invalid_argument);
  REQUIRE_THROWS_AS(request("GET", ""), std::invalid_argument);
}

TEST_CASE("Cookies", "[webkit][http]")
{
  SECTION("Cookie header")
  {
    auto c = parse_cookie_header("a=1; b=\"two\";c=; =bad; d=4");
    REQUIRE(c.size() == 4);
    REQUIRE(c["a"] == "1");
    REQUIRE(c["b"] == "two");
    REQUIRE(c["c"].empty());
    REQUIRE(c["d"] == "4");
  }

  SECTION("Set-Cookie value")
  {
    cookie c{"id", "42"};
    c.max_age = std::chrono::seconds(60);
    c.same_site = cookie::same_site_policy::lax;
    c.http_only = true;
    REQUIRE(c.str() == "id=42; Path=/; Max-Age=60; SameSite=Lax; HttpOnly");
  }
}

TEST_CASE("Messages", "[webkit][http]")
{
  SECTION("Request target")
  {
    request req("GET", "/users/12?sort=name&page=2");
    REQUIRE(req.path() == "/users/12");
    REQUIRE(req.query_string() == "sort=name&page=2");
    REQUIRE(req.query().find("page")->second == "2");
  }

  SECTION("Request from uri")
  {
    request req("GET", uri("http://example.com:8080/a?b=c"));
    REQUIRE(req.target() == "/a?b=c");
    REQUIRE(req.headers().get("Host") == "example.com:8080");
  }

  SECTION("Keep alive")
  {
    request req;
    REQUIRE(req.keep_alive());

    req.headers().set("Connection", "close");
    REQUIRE_FALSE(req.keep_alive());

    req.headers().erase("Connection");
    req.version(http_1_0);
    REQUIRE_FALSE(req.keep_alive());
  }
}

TEST_CASE("Serialization", "[webkit][http]")
{
  SECTION("Request")
  {
    request req("POST", "/items");
    req.headers().set("Host", "example.com");
    req.body("hello");

    REQUIRE(serialize(req) == "POST /items HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello");
  }

  SECTION("Response")
  {
    response res(status::not_found, "nope");
    REQUIRE(serialize(res) == "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\n\r\nnope");
  }

  SECTION("Bodyless response")
  {
    response res(status::no_content);
    REQUIRE(serialize(res) == "HTTP/1.1 204 No Content\r\n\r\n");
  }

  SECTION("Chunked response")
  {
    response res(status::ok, "hello");
    res.headers().set("Transfer-Encoding", "chunked");
    REQUIRE(serialize(res) == "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n");
  }
}
