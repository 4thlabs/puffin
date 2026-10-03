#ifndef PUFFIN_REST_TESTS_BANK_API_HPP
#define PUFFIN_REST_TESTS_BANK_API_HPP

#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// A small multi-domain api shared by the client and the server tests

namespace bank {

struct user {
  int id = 0;
  std::string name;
  std::string email;
};

struct new_user {
  std::string name;
  std::string email;
};

enum class card_status { active, frozen, cancelled };

struct card {
  int id = 0;
  int owner_id = 0;
  card_status status = card_status::active;
  std::int64_t limit_cents = 0;
};

struct new_card {
  std::int64_t limit_cents = 0;
};

struct credentials {
  std::string email;
  std::string password;
};

struct token {
  std::string value;
};

NLOHMANN_JSON_SERIALIZE_ENUM(card_status, {{card_status::active, "active"},
                                           {card_status::frozen, "frozen"},
                                           {card_status::cancelled, "cancelled"}})
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(user, id, name, email)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(new_user, name, email)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(card, id, owner_id, status, limit_cents)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(new_card, limit_cents)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(credentials, email, password)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(token, value)

} // namespace bank

template<>
struct puffin::rest::param_traits<bank::card_status>
    : puffin::rest::enum_param<bank::card_status, "active", "frozen", "cancelled"> {};

namespace bank {

namespace rest = puffin::rest;

namespace auth {
using namespace puffin::rest;

using login = endpoint<"auth.login", POST, "/login", body<credentials>, returns<token>>;
using me = endpoint<"auth.me", GET, "/me", security<bearer_auth>, returns<user>>;

using api = rest::api<"/auth", login, me>;
} // namespace auth

namespace users {
using namespace puffin::rest;

using list = endpoint<"users.list", GET, "/", query<"limit", std::optional<int>>,
                     query<"name", std::optional<std::string>>, returns<std::vector<user>>>;
using get = endpoint<"users.get", GET, "/{id:int}", returns<user>>;
using create = endpoint<"users.create", POST, "/", body<new_user>, returns<user, status::created>>;
using remove = endpoint<"users.remove", DELETE, "/{id:int}">;

using api = rest::api<"/users", list, get, create, remove>;
} // namespace users

namespace cards {
using namespace puffin::rest;

using list = endpoint<"cards.list", GET, "/", query<"status", std::optional<card_status>>,
                     returns<std::vector<card>>>;
using issue = endpoint<"cards.issue", POST, "/", body<new_card>, returns<card, status::created>>;
using freeze = endpoint<"cards.freeze", POST, "/{card_id:int}/freeze", returns<card>>;

using api = rest::api<"/users/{user_id:int}/cards", list, issue, freeze>;
} // namespace cards

using v1 = rest::api<"/api/v1", auth::api,
                     rest::with<rest::security<rest::api_key<"X-Api-Key">>, rest::header<"X-Request-Id", std::optional<std::string>>,
                                users::api, cards::api>>;

/// The data of the services, in memory
struct store {
  std::map<int, user> users;
  std::map<int, card> cards;
  int next_id = 1;
  std::vector<std::string> request_ids;
};

using puffin::async::async;
using rest::http_error;

class auth_service {
public:
  explicit auth_service(store& db)
      : db_(db)
  {}

  token operator()(auth::login, credentials c)
  {
    if (c.password != "secret")
      throw http_error(rest::status::unauthorized, "invalid credentials");

    return token {"token-" + c.email};
  }

  async<user> operator()(auth::me, auto& ctx)
  {
    auto bearer = rest::param_traits<rest::bearer>::parse(*ctx.request().headers().get("Authorization"));

    for (auto& [id, u] : db_.users) {
      if ("token-" + u.email == bearer->token)
        co_return u;
    }

    throw http_error(rest::status::not_found);
  }

private:
  store& db_;
};

class users_service {
public:
  explicit users_service(store& db)
      : db_(db)
  {}

  std::vector<user> operator()(users::list, std::optional<int> limit, std::optional<std::string> name,
                               std::optional<std::string> request_id)
  {
    if (request_id)
      db_.request_ids.push_back(*request_id);

    std::vector<user> result;

    for (auto& [id, u] : db_.users) {
      if (result.size() == static_cast<std::size_t>(limit.value_or(100)))
        break;

      if (!name || u.name == *name)
        result.push_back(u);
    }

    return result;
  }

  async<user> operator()(users::get, int id, std::optional<std::string>)
  {
    auto it = db_.users.find(id);

    if (it == db_.users.end())
      throw http_error(rest::status::not_found, "no user " + std::to_string(id));

    co_return it->second;
  }

  async<user> operator()(users::create, new_user u, std::optional<std::string>)
  {
    user created {db_.next_id++, std::move(u.name), std::move(u.email)};
    db_.users[created.id] = created;
    co_return created;
  }

  void operator()(users::remove, int id, std::optional<std::string>) { db_.users.erase(id); }

private:
  store& db_;
};

class cards_service {
public:
  explicit cards_service(store& db)
      : db_(db)
  {}

  std::vector<card> operator()(cards::list, int user_id, std::optional<card_status> status, std::optional<std::string>)
  {
    std::vector<card> result;

    for (auto& [id, c] : db_.cards) {
      if (c.owner_id == user_id && (!status || c.status == *status))
        result.push_back(c);
    }

    return result;
  }

  async<card> operator()(cards::issue, int user_id, new_card c, std::optional<std::string>)
  {
    if (!db_.users.count(user_id))
      throw http_error(rest::status::not_found, "no user " + std::to_string(user_id));

    card created {db_.next_id++, user_id, card_status::active, c.limit_cents};
    db_.cards[created.id] = created;
    co_return created;
  }

  async<card> operator()(cards::freeze, int user_id, int card_id, std::optional<std::string>)
  {
    auto it = db_.cards.find(card_id);

    if (it == db_.cards.end() || it->second.owner_id != user_id)
      throw http_error(rest::status::not_found, "no card " + std::to_string(card_id));

    it->second.status = card_status::frozen;
    co_return it->second;
  }

private:
  store& db_;
};

/// Accepts one API key
struct api_key_validator {
  std::string key;

  bool operator()(const rest::api_key_value& v) const { return v.value == key; }
};

/// Accepts the tokens issued by auth::login
struct bearer_validator {
  async<bool> operator()(rest::bearer b, auto&) const { co_return b.token.rfind("token-", 0) == 0; }
};

} // namespace bank

#endif // PUFFIN_REST_TESTS_BANK_API_HPP
