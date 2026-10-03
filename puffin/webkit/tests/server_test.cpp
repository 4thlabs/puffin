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

#include <puffin/webkit/middlewares/cookies.hpp>
#include <puffin/webkit/middlewares/session.hpp>
#include <puffin/webkit/server/context.hpp>
#include <puffin/webkit/server/middleware.hpp>
#include <puffin/webkit/server/router.hpp>
#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace puffin::webkit;

TEST_CASE("Router", "[webkit][router]")
{
  basic_router<std::function<int()>> router;
  router.get("/users", [] { return 1; });
  router.get("/users/([0-9]+)", [] { return 2; });
  router.post("/users", [] { return 3; });

  SECTION("Match by method and path")
  {
    REQUIRE((*router.find("GET", "/users").handler)() == 1);
    REQUIRE((*router.find("POST", "/users").handler)() == 3);
  }

  SECTION("Route parameters")
  {
    auto m = router.find("GET", "/users/42");
    REQUIRE(m);
    REQUIRE((*m.handler)() == 2);
    REQUIRE(m.params == std::vector<std::string>{"42"});
  }

  SECTION("Not found")
  {
    auto m = router.find("GET", "/users/abc");
    REQUIRE_FALSE(m);
    REQUIRE(m.allowed_methods.empty());
  }

  SECTION("Method not allowed")
  {
    auto m = router.find("DELETE", "/users");
    REQUIRE_FALSE(m);
    REQUIRE(m.allowed_methods == std::vector<std::string>{"GET", "POST"});
  }

  SECTION("HEAD falls back to GET")
  {
    REQUIRE((*router.find("HEAD", "/users/1").handler)() == 2);
  }
}

namespace {

struct trace {
  struct data_type {
    std::vector<std::string> events;
  };
};

/// Records its calls in the trace middleware data
template<char Name, bool Accept = true>
struct recorder {
  template<typename Context>
  bool before(Context& ctx)
  {
    ctx.template data<trace>().events.push_back(std::string("before ") + Name);
    return Accept;
  }

  template<typename Context>
  void after(Context& ctx)
  {
    ctx.template data<trace>().events.push_back(std::string("after ") + Name);
  }
};

/// A middleware without any hook nor data
struct inert {};

} // namespace

TEST_CASE("Middleware chain", "[webkit][middleware]")
{
  request req;
  response res;

  SECTION("before in order, after in reverse order")
  {
    middleware_chain<trace, recorder<'a'>, inert, recorder<'b'>> chain;
    decltype(chain)::context_type ctx(req, res);

    std::size_t entered;
    REQUIRE(chain.before(ctx, entered));
    REQUIRE(entered == 4);
    chain.after(ctx, entered);

    REQUIRE(ctx.events == std::vector<std::string>{"before a", "before b", "after b", "after a"});
  }

  SECTION("A middleware can stop the request")
  {
    middleware_chain<trace, recorder<'a'>, recorder<'b', false>, recorder<'c'>> chain;
    decltype(chain)::context_type ctx(req, res);

    std::size_t entered;
    REQUIRE_FALSE(chain.before(ctx, entered));
    chain.after(ctx, entered);

    REQUIRE(ctx.events == std::vector<std::string>{"before a", "before b", "after b", "after a"});
  }
}

TEST_CASE("Cookies and session middlewares", "[webkit][middleware]")
{
  using chain_type = middleware_chain<middlewares::cookies, middlewares::session>;

  middlewares::session::options opts;
  opts.secret = std::string(32, 'k');
  middlewares::session signer(opts);
  chain_type chain(middlewares::cookies{}, middlewares::session(opts));

  request req("GET", "/");
  req.headers().add("Cookie", "theme=dark; puffin_session=" + signer.sign("user%20name=bob&role=admin"));
  response res;

  chain_type::context_type ctx(req, res);
  std::size_t entered;
  REQUIRE(chain.before(ctx, entered));

  SECTION("Request cookies and session are exposed by the context")
  {
    REQUIRE(ctx.cookie("theme") == "dark");
    REQUIRE_FALSE(ctx.cookie("missing"));
    REQUIRE(ctx.session().at("user name") == "bob");
    REQUIRE(ctx.session().at("role") == "admin");
  }

  SECTION("Unchanged session is not sent back")
  {
    chain.after(ctx, entered);
    REQUIRE(res.headers().get_all("Set-Cookie").empty());
  }

  SECTION("Set cookies and modified session are sent back signed")
  {
    ctx.set_cookie(cookie{"lang", "fr"});
    ctx.session()["role"] = "guest";
    chain.after(ctx, entered);

    auto set_cookies = res.headers().get_all("Set-Cookie");
    REQUIRE(set_cookies.size() == 2);

    REQUIRE(set_cookies[0].starts_with("puffin_session="));
    auto value = std::string_view(set_cookies[0]).substr(15);
    value = value.substr(0, value.find(';'));
    REQUIRE(signer.verify(value) == "role=guest&user%20name=bob");
    REQUIRE(set_cookies[1] == "lang=fr; Path=/");
  }

  SECTION("Cleared session expires the cookie")
  {
    ctx.session().clear();
    chain.after(ctx, entered);
    REQUIRE(res.headers().get("Set-Cookie")->find("Max-Age=0") != std::string_view::npos);
  }

  SECTION("Oversized session fails instead of producing a cookie browsers drop")
  {
    ctx.session()["big"] = std::string(5000, 'x');
    REQUIRE_THROWS_AS(chain.after(ctx, entered), std::length_error);
  }
}

TEST_CASE("Session cookies are signed", "[webkit][middleware]")
{
  using chain_type = middleware_chain<middlewares::session>;

  middlewares::session::options opts;
  opts.secret = std::string(32, 'k');
  middlewares::session signer(opts);
  chain_type chain(middlewares::session{opts});

  auto session_of = [&](std::string cookie) {
    request req("GET", "/");
    req.headers().add("Cookie", "puffin_session=" + cookie);
    response res;
    chain_type::context_type ctx(req, res);
    std::size_t entered;
    chain.before(ctx, entered);
    return ctx.session();
  };

  auto valid = signer.sign("role=admin");
  REQUIRE(session_of(valid).at("role") == "admin");

  SECTION("Unsigned, forged or tampered cookies are ignored")
  {
    REQUIRE(session_of("role=admin").empty());
    REQUIRE(session_of("role=admin.").empty());
    REQUIRE(session_of("1.role=admin.").empty());

    auto first = valid.find('.');
    auto last = valid.rfind('.');
    REQUIRE(session_of(valid.substr(0, first) + ".role=root" + valid.substr(last)).empty());
    REQUIRE(session_of("9" + valid).empty());
    REQUIRE(session_of(valid + "x").empty());
  }

  SECTION("Expired cookies are ignored, even with a valid signature")
  {
    auto now = middlewares::session::clock::now();
    auto expired = signer.sign("role=admin", now - std::chrono::seconds(1));
    REQUIRE_FALSE(signer.verify(expired, now));
    REQUIRE(session_of(expired).empty());
    REQUIRE(signer.verify(signer.sign("role=admin", now + std::chrono::seconds(10)), now) == "role=admin");
  }

  SECTION("A value signed for another cookie name does not validate")
  {
    auto other_opts = opts;
    other_opts.cookie_name = "other_session";
    REQUIRE(session_of(middlewares::session(other_opts).sign("role=admin")).empty());
  }

  SECTION("Another key does not validate")
  {
    middlewares::session other(middlewares::session::options{.secret = std::string(32, 'o')});
    REQUIRE(session_of(other.sign("role=admin")).empty());
  }

  SECTION("Short secrets are refused")
  {
    REQUIRE_THROWS_AS(middlewares::session(middlewares::session::options{.secret = "short"}), std::invalid_argument);
  }

  SECTION("Default instances get distinct random keys")
  {
    REQUIRE(middlewares::session().sign("a") != middlewares::session().sign("a"));
  }
}
