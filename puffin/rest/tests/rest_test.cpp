#include "bank_api.hpp"

#include <puffin/async.hpp>
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>
#include <puffin/webkit/interceptors/bearer.hpp>
#include <puffin/webkit/interceptors/retry.hpp>
#include <puffin/webkit/server/server.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace pa = puffin::async;
namespace wk = puffin::webkit;
namespace rest = puffin::rest;

//
// Compile time description
//

static_assert(rest::join_path<"/api/v1", "/users/">.view() == "/api/v1/users");
static_assert(rest::join_path<"", "/">.view() == "/");
static_assert(rest::join_path<"api", "x">.view() == "/api/x");

using freeze_t = rest::find_endpoint_t<bank::v1, bank::cards::freeze>;
static_assert(freeze_t::path.view() == "/api/v1/users/{user_id:int}/cards/{card_id:int}/freeze");
static_assert(std::is_same_v<freeze_t::values, rest::detail::typelist<int, int, std::optional<std::string>>>);
static_assert(std::is_same_v<freeze_t::result_type, bank::card>);
static_assert(rest::detail::contains_v<rest::api_key<"X-Api-Key">, freeze_t::schemes>);
static_assert(!rest::detail::contains_v<rest::api_key<"X-Api-Key">, rest::find_endpoint_t<bank::v1, bank::auth::login>::schemes>);
static_assert(rest::find_endpoint_t<bank::v1, bank::users::remove>::returns::status == rest::status::no_content);
static_assert(bank::v1::endpoints::size == 9);
static_assert(rest::path_template<"/files/{p:path}">::param_count == 1);
static_assert(rest::path_template<"/a/{x}/{y:bool}">::param_name(1) == "y");

namespace named {
using namespace puffin::rest;

// Same method and path in two domains: distinct types thanks to their name
namespace users {
using remove = endpoint<"users.remove", DELETE, "/{id:int}">;
using api = rest::api<"/users", remove>;
} // namespace users

namespace cards {
using remove = endpoint<"cards.remove", DELETE, "/{id:int}">;
using api = rest::api<"/users/{user_id:int}/cards", remove>;
} // namespace cards

static_assert(!std::is_same_v<users::remove, cards::remove>);

using api = rest::api<"/v1", users::api, cards::api>;
static_assert(find_endpoint_t<api, cards::remove>::path.view() == "/v1/users/{user_id:int}/cards/{id:int}");
static_assert(find_endpoint_t<api, cards::remove>::name == "cards.remove");
static_assert(find_endpoint_t<bank::v1, bank::users::get>::name == "users.get");

struct users_service {
  std::vector<std::string>* removed;
  void operator()(users::remove, int id) { removed->push_back("user " + std::to_string(id)); }
};

struct cards_service {
  std::vector<std::string>* removed;
  void operator()(cards::remove, int user_id, int id)
  {
    removed->push_back("card " + std::to_string(id) + " of " + std::to_string(user_id));
  }
};
} // namespace named

TEST_CASE("Endpoints with the same method and path", "[rest]")
{
  std::vector<std::string> removed;
  wk::basic_server<> server;
  rest::mount<named::api>(server, named::users_service {&removed}, named::cards_service {&removed});

  rest::client<named::api, rest::local_transport<wk::basic_server<>>> api(server);

  pa::sync_wait([&]() -> pa::async<void> {
    co_await api.call<named::users::remove>(1);
    co_await api.call<named::cards::remove>(7, 2);
  }());

  REQUIRE(removed == std::vector<std::string> {"user 1", "card 2 of 7"});
}

TEST_CASE("Path templates", "[rest]")
{
  using t = rest::path_template<"/users/{id:int}/files/{name}/{rest:path}">;

  REQUIRE(t::regex() == "/users/(-?[0-9]+)/files/([^/]+)/(.+)");
  REQUIRE(t::format(42, std::string("a b"), std::string("x/y z")) == "/users/42/files/a%20b/x/y%20z");
  REQUIRE(t::parse<0>("-3") == -3);
  REQUIRE_FALSE(t::parse<0>("3x"));
  REQUIRE(t::parse<1>("a%20b") == "a b");
  REQUIRE(rest::path_template<"/v1.0/{id}">::regex() == "/v1\\.0/([^/]+)");
}

TEST_CASE("Parameter traits", "[rest]")
{
  REQUIRE(rest::param_traits<int>::parse("12") == 12);
  REQUIRE_FALSE(rest::param_traits<int>::parse(""));
  REQUIRE_FALSE(rest::param_traits<int>::parse("1.5"));
  REQUIRE(rest::param_traits<double>::parse("1.5") == 1.5);
  REQUIRE(rest::param_traits<bool>::parse("true") == true);
  REQUIRE(rest::param_traits<bank::card_status>::parse("frozen") == bank::card_status::frozen);
  REQUIRE_FALSE(rest::param_traits<bank::card_status>::parse("lost"));
  REQUIRE(rest::param_traits<bank::card_status>::format(bank::card_status::cancelled) == "cancelled");
  REQUIRE(rest::param_traits<rest::bearer>::parse("bearer abc")->token == "abc");
  REQUIRE_FALSE(rest::param_traits<rest::bearer>::parse("Basic abc"));
}

namespace {

using server_type = wk::basic_server<>;

struct bank_fixture {
  bank::store db;
  server_type server;

  bank_fixture()
  {
    rest::mount<bank::v1>(server, bank::auth_service {db}, bank::users_service {db}, bank::cards_service {db},
                          rest::validators(bank::api_key_validator {"k3y"}, bank::bearer_validator {}));
  }

  wk::response send(wk::request req)
  {
    return pa::sync_wait([](server_type& s, wk::request r) -> pa::async<wk::response> {
      wk::response res;
      co_await s.handle(r, res);
      co_return res;
    }(server, std::move(req)));
  }

  wk::request with_key(wk::request req)
  {
    req.headers().set("X-Api-Key", "k3y");
    return req;
  }
};

using local_client = rest::client<bank::v1, rest::local_transport<server_type>>;

} // namespace

TEST_CASE("Server: requests, statuses and errors", "[rest]")
{
  bank_fixture f;

  SECTION("create and get")
  {
    wk::request create = f.with_key(wk::request("POST", "/api/v1/users"));
    create.headers().set("Content-Type", "application/json");
    create.body(R"({"name":"Ada","email":"ada@bank.io"})");

    wk::response res = f.send(create);
    REQUIRE(res.status_code() == 201);
    REQUIRE(res.headers().get("Content-Type") == "application/json");
    REQUIRE(nlohmann::json::parse(res.body())["name"] == "Ada");

    res = f.send(f.with_key(wk::request("GET", "/api/v1/users/1")));
    REQUIRE(res.status_code() == 200);
    REQUIRE(nlohmann::json::parse(res.body())["email"] == "ada@bank.io");

    res = f.send(f.with_key(wk::request("DELETE", "/api/v1/users/1")));
    REQUIRE(res.status_code() == 204);
    REQUIRE(res.body().empty());
  }

  SECTION("http_error thrown by a service")
  {
    wk::response res = f.send(f.with_key(wk::request("GET", "/api/v1/users/9")));
    REQUIRE(res.status_code() == 404);
    REQUIRE(res.body() == R"({"error":"no user 9"})");
  }

  SECTION("errors written by another error codec")
  {
    bank::store db;
    server_type server;
    rest::mount<bank::v1>(server, bank::auth_service {db}, bank::users_service {db}, bank::cards_service {db},
                          rest::validators(bank::api_key_validator {"k3y"}, bank::bearer_validator {}),
                          rest::errors(rest::text_error_codec {}));

    wk::request req = f.with_key(wk::request("GET", "/api/v1/users/9"));
    wk::response res;
    pa::sync_wait(server.handle(req, res));

    REQUIRE(res.status_code() == 404);
    REQUIRE(res.body() == "no user 9");
    REQUIRE(res.headers().get("Content-Type") == "text/plain; charset=utf-8");
  }

  SECTION("invalid arguments")
  {
    REQUIRE(f.send(f.with_key(wk::request("GET", "/api/v1/users?limit=abc"))).status_code() == 400);
    REQUIRE(f.send(f.with_key(wk::request("GET", "/api/v1/users/1/cards?status=lost"))).status_code() == 400);

    wk::request bad_body = f.with_key(wk::request("POST", "/api/v1/users"));
    bad_body.body("{not json");
    wk::response res = f.send(bad_body);
    REQUIRE(res.status_code() == 400);
    REQUIRE(res.body().find("invalid body") != std::string::npos);

    wk::request bad_type = f.with_key(wk::request("POST", "/api/v1/users"));
    bad_type.headers().set("Content-Type", "text/plain");
    bad_type.body("{}");
    REQUIRE(f.send(bad_type).status_code() == 415);
  }

  SECTION("routing")
  {
    // The path does not match the int parameter
    REQUIRE(f.send(f.with_key(wk::request("GET", "/api/v1/users/abc"))).status_code() == 404);
    REQUIRE(f.send(f.with_key(wk::request("PUT", "/api/v1/users/1"))).status_code() == 405);
  }

  SECTION("security")
  {
    REQUIRE(f.send(wk::request("GET", "/api/v1/users")).status_code() == 401);

    wk::request wrong("GET", "/api/v1/users");
    wrong.headers().set("X-Api-Key", "nope");
    REQUIRE(f.send(wrong).status_code() == 401);

    REQUIRE(f.send(f.with_key(wk::request("GET", "/api/v1/users"))).status_code() == 200);

    // auth::login is outside the secured group
    wk::request login("POST", "/api/v1/auth/login");
    login.body(R"({"email":"a","password":"secret"})");
    REQUIRE(f.send(login).status_code() == 200);
  }
}

TEST_CASE("Client: calls through a transport", "[rest]")
{
  bank_fixture f;
  local_client api(f.server);
  api.credentials<rest::api_key<"X-Api-Key">>("k3y");
  api.headers().set("User-Agent", "bank-test");

  auto scenario = [&]() -> pa::async<void> {
    bank::user ada = co_await api.call<bank::users::create>(bank::new_user {"Ada", "ada@bank.io"});
    REQUIRE(ada.id == 1);

    // Trailing optional arguments omitted
    bank::user got = co_await api.call<bank::users::get>(ada.id);
    REQUIRE(got.name == "Ada");

    co_await api.call<bank::users::create>(bank::new_user {"Bob", "bob@bank.io"}, "req-1");
    REQUIRE(f.db.request_ids.empty());

    auto all = co_await api.call<bank::users::list>();
    REQUIRE(all.size() == 2);

    auto bobs = co_await api.call<bank::users::list>(std::nullopt, "Bob", "req-2");
    REQUIRE(bobs.size() == 1);
    REQUIRE(f.db.request_ids == std::vector<std::string> {"req-2"});

    // Scoped on a user
    auto ada_cards = api.scope<bank::cards::api>(ada.id);
    bank::card c = co_await ada_cards.call<bank::cards::issue>(bank::new_card {50000});
    co_await ada_cards.call<bank::cards::issue>(bank::new_card {100});
    bank::card frozen = co_await ada_cards.call<bank::cards::freeze>(c.id);
    REQUIRE(frozen.status == bank::card_status::frozen);

    auto frozen_cards = co_await ada_cards.call<bank::cards::list>(bank::card_status::frozen);
    REQUIRE(frozen_cards.size() == 1);
    auto r1 = co_await ada_cards.call<bank::cards::list>();
    REQUIRE(r1.size() == 2);

    // Errors
    bool thrown = false;

    try {
      co_await api.call<bank::users::get>(99);
    } catch (const rest::http_error& e) {
      thrown = true;
      REQUIRE(e.status() == 404);
      REQUIRE(e.body() == R"({"error":"no user 99"})");
    }

    REQUIRE(thrown);

    rest::result<bank::user> missing = co_await api.try_call<bank::users::get>(99);
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error().status() == 404);

    rest::result<void> removed = co_await api.try_call<bank::users::remove>(2);
    REQUIRE(removed);

    // Without the key
    api.credentials<rest::api_key<"X-Api-Key">>("wrong");
    auto r2 = co_await api.try_call<bank::users::list>();
    REQUIRE(r2.error().status() == 401);
  };

  pa::sync_wait(scenario());
}

TEST_CASE("Client: encoding errors come out of the task", "[rest]")
{
  bank_fixture f;
  local_client api(f.server);
  api.credentials<rest::api_key<"X-Api-Key">>("k3y");

  // A header value with a line feed is rejected when the request is built, inside the task
  auto call = api.call<bank::cards::list>(7, bank::card_status::frozen, "bad\nid");
  REQUIRE_THROWS_AS(pa::sync_wait(std::move(call)), std::invalid_argument);

  auto attempt = api.try_call<bank::cards::list>(7, bank::card_status::frozen, "bad\nid");
  REQUIRE_THROWS_AS(pa::sync_wait(std::move(attempt)), std::invalid_argument);
}

TEST_CASE("Client: request building", "[rest]")
{
  bank_fixture f;
  local_client api(f.server);

  wk::request req = api.make_request<bank::cards::list>(7, bank::card_status::frozen, "id 1");
  REQUIRE(req.method() == "GET");
  REQUIRE(req.target() == "/api/v1/users/7/cards?status=frozen");
  REQUIRE(req.headers().get("X-Request-Id") == "id 1");
  REQUIRE(req.headers().get("Accept") == "application/json");

  req = api.make_request<bank::users::list>(10, "Ada Lovelace");
  REQUIRE(req.target() == "/api/v1/users?limit=10&name=Ada%20Lovelace");

  req = api.make_request<bank::auth::login>(bank::credentials {"a", "b"});
  REQUIRE(req.method() == "POST");
  REQUIRE(req.headers().get("Content-Type") == "application/json");
  REQUIRE(nlohmann::json::parse(req.body())["password"] == "b");
}

namespace {

/// Records the calls and their status
struct recorder {
  std::vector<std::string>* log;

  pa::async<wk::response> operator()(wk::request& req, wk::next_request next) const
  {
    wk::response res = co_await next(req);
    log->push_back(req.method() + " " + std::string(req.path()) + " " + std::to_string(res.status_code()));
    co_return res;
  }
};

/// Fails the first attempts with 503
struct flaky {
  int failures = 0;

  pa::async<wk::response> operator()(wk::request& req, wk::next_request next)
  {
    if (failures > 0) {
      --failures;
      co_return wk::response(wk::status::service_unavailable);
    }

    co_return co_await next(req);
  }
};

/// Logs the endpoint name and the status the rest layer answered
struct server_audit {
  std::vector<std::string>* log = nullptr;

  template<typename Context>
  pa::async<void> around(Context& ctx, wk::next_handler next)
  {
    co_await next();
    log->push_back((ctx.route() ? ctx.route()->name : "-") + " " + std::to_string(ctx.response().status_code()));
  }
};

/// Answers 503 itself while enabled
struct maintenance {
  bool enabled = false;

  template<typename Context>
  pa::async<void> around(Context& ctx, wk::next_handler next)
  {
    if (enabled) {
      ctx.response().status(wk::status::service_unavailable);
      co_return;
    }

    co_await next();
  }
};

} // namespace

TEST_CASE("Interceptors and middlewares", "[rest]")
{
  SECTION("client")
  {
    bank_fixture f;
    std::vector<std::string> log;

    rest::client<bank::v1, rest::local_transport<server_type>, recorder, wk::interceptors::retry, flaky,
                 wk::interceptors::bearer>
        api(f.server);
    api.interceptor<recorder>().log = &log;
    api.credentials<rest::api_key<"X-Api-Key">>("k3y");

    pa::sync_wait([&]() -> pa::async<void> {
      co_await api.call<bank::users::create>(bank::new_user {"Ada", "ada@bank.io"});

      // Retried twice on 503, recorded once
      api.interceptor<flaky>().failures = 2;
      auto r3 = co_await api.call<bank::users::get>(1);
      REQUIRE(r3.name == "Ada");

      // Not retried: POST is not idempotent
      api.interceptor<flaky>().failures = 1;
      auto r4 = co_await api.try_call<bank::users::create>(bank::new_user {"Bob", "b"});
      REQUIRE(r4.error().status() == 503);
      api.interceptor<flaky>().failures = 0;

      // Bearer, refreshed on 401
      bank::token t = co_await api.call<bank::auth::login>(bank::credentials {"ada@bank.io", "secret"});
      int refreshed = 0;
      api.interceptor<wk::interceptors::bearer>().token("expired");
      api.interceptor<wk::interceptors::bearer>().on_refresh([&]() -> pa::async<std::string> {
        ++refreshed;
        co_return t.value;
      });

      auto r5 = co_await api.call<bank::auth::me>();
      REQUIRE(r5.name == "Ada");
      REQUIRE(refreshed == 1);
    }());

    REQUIRE(log == std::vector<std::string> {"POST /api/v1/users 201", "GET /api/v1/users/1 200",
                                             "POST /api/v1/users 503", "POST /api/v1/auth/login 200",
                                             "GET /api/v1/auth/me 200"});
  }

  SECTION("server")
  {
    bank::store db;
    std::vector<std::string> log;
    wk::basic_server<server_audit, maintenance> server;
    server.middleware<server_audit>().log = &log;

    rest::mount<bank::v1>(server, bank::auth_service {db}, bank::users_service {db}, bank::cards_service {db},
                          rest::validators(bank::api_key_validator {"k3y"}, bank::bearer_validator {}));

    rest::client<bank::v1, rest::local_transport<decltype(server)>> api(server);

    pa::sync_wait([&]() -> pa::async<void> {
      auto r6 = co_await api.try_call<bank::users::list>();
      REQUIRE(r6.error().status() == 401);

      api.credentials<rest::api_key<"X-Api-Key">>("k3y");
      co_await api.call<bank::users::list>();

      server.middleware<maintenance>().enabled = true;
      auto r = co_await api.try_call<bank::users::list>();
      REQUIRE(r.error().status() == 503);
    }());

    // The middlewares see the errors answered by rest, and the endpoint names
    REQUIRE(log == std::vector<std::string> {"users.list 401", "users.list 200", "users.list 503"});
  }
}

namespace creds {
using namespace puffin::rest;

// Schemes sharing a name or a header: each keeps its own credential
using header_key = endpoint<"header_key", GET, "/h", security<api_key<"key">>>;
using query_key = endpoint<"query_key", GET, "/q", security<api_key_query<"key">>>;
using authorization_key = endpoint<"authorization_key", GET, "/a", security<api_key<"Authorization">>>;
using token = endpoint<"token", GET, "/t", security<bearer_auth>>;
using open = endpoint<"open", GET, "/o">;

using api = rest::api<"", header_key, query_key, authorization_key, token, open>;
} // namespace creds

TEST_CASE("Client credentials", "[rest]")
{
  bank_fixture f;
  rest::client<creds::api, rest::local_transport<server_type>> api(f.server);
  api.credentials<rest::api_key<"key">>("in header");
  api.credentials<rest::api_key_query<"key">>("in query");
  api.credentials<rest::api_key<"Authorization">>("raw");
  api.credentials<rest::bearer_auth>("t0k");

  REQUIRE(api.make_request<creds::header_key>().headers().get("key") == "in header");
  REQUIRE(api.make_request<creds::query_key>().target() == "/q?key=in%20query");
  REQUIRE(api.make_request<creds::authorization_key>().headers().get("Authorization") == "raw");
  REQUIRE(api.make_request<creds::token>().headers().get("Authorization") == "Bearer t0k");

  wk::request open = api.make_request<creds::open>();
  REQUIRE_FALSE(open.headers().contains("key"));
  REQUIRE_FALSE(open.headers().contains("Authorization"));
  REQUIRE(open.target() == "/o");
}
