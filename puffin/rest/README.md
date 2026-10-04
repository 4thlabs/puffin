# puffin::rest

A DSL describing a REST api once, as C++ types, to serve it on a `puffin::webkit` server and to call it with a client.
Paths, arguments and services are checked at compile time. Header only.

- Target: `puffin::rest`
- Header: `<puffin/rest.hpp>`
- JSON bodies: `puffin::rest_json` (`<puffin/rest/json.hpp>`, nlohmann::json)

```cmake
target_link_libraries(my_app PRIVATE puffin::rest puffin::rest_json)
```

## Describing an api

```c++
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>

namespace rest = puffin::rest;

namespace users {
using namespace puffin::rest;

using list   = endpoint<"users.list",   GET,    "/",         query<"limit", std::optional<int>>, returns<std::vector<user>>>;
using get    = endpoint<"users.get",    GET,    "/{id:int}", returns<user>>;
using create = endpoint<"users.create", POST,   "/",         body<new_user>, returns<user, status::created>>;
using remove = endpoint<"users.remove", DELETE, "/{id:int}">; // 204

using api = rest::api<"/users", list, get, create, remove>;
}

namespace cards {
using namespace puffin::rest;

using list   = endpoint<"cards.list",   GET,    "/", returns<std::vector<card>>>;
using remove = endpoint<"cards.remove", DELETE, "/{id:int}">;

using api = rest::api<"/users/{user_id:int}/cards", list, remove>;
}

using v1 = rest::api<"/api/v1",
                     auth::api,
                     rest::with<rest::security<rest::api_key<"X-Api-Key">>, users::api, cards::api>>;
```

- `endpoint<"name", METHOD, "/path", parts...>`: the name is mandatory and unique in its api (checked at compile
  time). It keeps endpoints with the same method and path in different domains distinct (`users.remove` and
  `cards.remove`), and the server names its route after it (`ctx.route()->name` in webkit middlewares).
- Path parameters `{name:type}`: `int`, `int64`, `uint`, `uint64`, `double`, `bool`, `string` (the default), `path`
  (the rest of the path).
- Parts: `query<"name", T>` (`std::optional<T>` when optional, `std::vector<T>` when repeated),
  `header<"Name", T>`, `body<T, Codec>`, `returns<T, status, Codec>`.
- `api<"/prefix", ...>` nests apis; prefixes may have parameters. `with<parts..., apis...>` adds parts to every
  endpoint of the apis it wraps.
- Security: `security<api_key<"Header">, api_key_query<"param">, bearer_auth>`. Schemes are not arguments: the server
  checks them with validators, the client sends the credentials it was given.

Arguments are the same on both sides: the path parameters, the endpoint parts in order, then the parts inherited
from `with<>`.

## Serving it

One `operator()(Endpoint, args...)` per endpoint, returning a value or an `async<>` of it. The webkit context may be
added as the last argument.

```c++
struct users_service {
  async<std::vector<user>> operator()(users::list, std::optional<int> limit);
  async<user> operator()(users::get, int id, auto& ctx);
  user operator()(users::create, new_user u);
  void operator()(users::remove, int id);
};

rest::mount<v1>(server, auth_service {}, users_service {}, cards_service {},
                rest::validators([&](const rest::api_key_value& k) { return k.value == key; }));
```

A missing service, or one with the wrong arguments, is a compile error. Each request goes through the webkit
middlewares of the server, then the security validators, argument decoding, the service and the response encoding:

- 400 for an invalid argument or body, 415 for a wrong `Content-Type`, 401 for missing or rejected credentials.
- `throw rest::http_error(status, "message")` answers that status with `{"error": "message"}`, the message is sent
  as is to the client; other exceptions answer 500 without detail.
- A `void` service answers 204, otherwise the status of `returns<>` (200 by default).
- Cross-cutting concerns (logs, metrics, maintenance) are webkit middlewares (`around(ctx, next)`, see the
  [webkit README](../webkit/README.md#middlewares)). They see the responses rest answered, errors included, and the
  endpoint name with `ctx.route()->name`.

## Calling it

```c++
#include <puffin/webkit/adapter/asio.hpp>

rest::client<v1, wk::asio::tcp_connector, wk::interceptors::retry> api(connector, "host", 80);
api.credentials<rest::api_key<"X-Api-Key">>("...");

user u = co_await api.call<users::get>(42);
auto page = co_await api.call<users::list>();                // trailing optionals may be omitted
rest::result<user> r = co_await api.try_call<users::get>(7); // errors as values instead of rest::http_error

auto alice_cards = api.scope<cards::api>(1);                 // fixes user_id
co_await alice_cards.call<cards::remove>(3);
```

- The client runs over a webkit `Connector` (wrapped in a `webkit::basic_client`) or any `Transport`;
  `local_transport` calls a server in process, for tests.
- `credentials<Scheme>(value)` gives the value sent for a security scheme (`api_key<>`, `api_key_query<>`,
  `bearer_auth`), only to the endpoints declaring it.
- Interceptors are webkit ones (`async<response> operator()(request&, next_request)`, see the
  [webkit README](../webkit/README.md#interceptors)): `wk::interceptors::retry`, `wk::interceptors::bearer`
  (refreshes the token once on 401)...
- `headers()` adds headers to every request, `make_request<E>(args...)` builds a request without sending it.

## Parameters and bodies

- `param_traits<T>` converts path, query and header values from and to text. Specialize it for your types;
  `enum_param<E, "a", "b">` maps an enum to names.
- A `Codec` encodes and decodes bodies: `text_codec` for strings, `json_codec` (rest_json) by default for structured
  types.

The full example is in [samples/rest_sample.cpp](../../samples/rest_sample.cpp).
