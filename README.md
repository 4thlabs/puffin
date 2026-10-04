<p align="center">
  <img src="./docs/logo.svg" height="128" alt="Puffin">
</p>

<h1 align="center">Puffin Cpp</h1>

<p align="center">
  Small, header-only C++20 building blocks to bootstrap projects faster:<br>
  coroutines, an HTTP server and client, an event bus, an IoC container and more.
</p>

<p align="center">
  <a href="https://github.com/4thlabs/puffin/actions/workflows/ci.yml"><img src="https://github.com/4thlabs/puffin/actions/workflows/ci.yml/badge.svg?branch=master" alt="CI"></a>
  <a href="LICENSE.md"><img src="https://img.shields.io/badge/license-BSD--3--Clause-blue.svg" alt="License: BSD 3-Clause"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg?logo=cplusplus" alt="C++20">
  <img src="https://img.shields.io/badge/header--only-yes-brightgreen.svg" alt="Header only">
  <img src="https://img.shields.io/badge/CMake-3.24%2B-064F8C.svg?logo=cmake" alt="CMake 3.24+">
</p>

---

## Modules

Each module is a separate CMake target, link only what you use.

| Module | Target | Description | Docs |
| --- | --- | --- | --- |
| **async** | `puffin::async` | C++20 coroutines independent of any event loop, with asio and Qt adapters. | [README](puffin/async/README.md) |
| **webkit** | `puffin::webkit` | HTTP/1.1 server and client on `puffin::async`, sans-IO core, asio (TCP, TLS) and Qt transports. | [README](puffin/webkit/README.md) |
| **rest** | `puffin::rest` | A DSL describing REST apis once, to serve them on webkit and to call them. | [README](puffin/rest/README.md) |
| **imdb** | `puffin::imdb` | In-memory database with fluent requests, views, indexes and JSON helpers. | [README](puffin/imdb/README.md) |
| **events** | `puffin::events` | Type-safe event bus. | [Example](#events) |
| **ioc** | `puffin::ioc` | Dependency injection container with typed bindings. | |
| **maths** | `puffin::maths` | `vector3`, `vector4`, `matrix4` and transforms. | |
| **common** | `puffin::common` | Type traits, tuple helpers, compile-time string and path literals. | |

Adapters are separate targets, built only when their dependency is found:

| Target | Requires |
| --- | --- |
| `puffin::async_asio` | asio (standalone) or Boost.Asio |
| `puffin::async_qt` | Qt 6 Core |
| `puffin::webkit_asio` | `puffin::async_asio` |
| `puffin::webkit_asio_ssl` | `puffin::webkit_asio` and OpenSSL |
| `puffin::webkit_qt` | `puffin::async_qt` and Qt 6 Network |
| `puffin::rest_json` | `puffin::rest` and nlohmann::json (JSON bodies) |

## Getting started

Requirements: a C++20 compiler (tested with GCC 13 and Clang 18) and CMake 3.24 or newer.

With `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(puffin
  GIT_REPOSITORY https://github.com/4thlabs/puffin.git
  GIT_TAG master
)
FetchContent_MakeAvailable(puffin)

target_link_libraries(my_app PRIVATE puffin::webkit_asio)
```

Or with a copy of the repository: `add_subdirectory(puffin)`.

## Building and testing

```sh
sudo apt-get install libasio-dev libssl-dev qt6-base-dev   # optional, for the adapters

cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_SAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| Option | Default | Description |
| --- | --- | --- |
| `BUILD_TESTS` | `OFF` | Builds the Catch2 test suite (`build/tests/main_test`). |
| `BUILD_SAMPLES` | `OFF` | Builds the samples in [`samples/`](samples). |
| `PUFFIN_ASYNC_WITH_ASIO` | `ON` | Builds the asio adapters when asio is found. |
| `PUFFIN_ASYNC_USE_BOOST_ASIO` | `OFF` | Uses Boost.Asio instead of standalone asio. |
| `PUFFIN_ASYNC_WITH_QT` | `ON` | Builds the Qt adapters when Qt 6 is found. |
| `PUFFIN_WEBKIT_WITH_SSL` | `ON` | Builds the TLS transport when OpenSSL is found. |

## A quick tour

Application code only uses puffin: routes, services and clients do not depend on asio or Qt. `main` picks the
runtime, creating the executor, the acceptors and the connectors, and the samples are written that way.

### Async

```c++
#include <puffin/async.hpp>

using namespace puffin::async;

async<int> answer() { co_return 42; }

async<int> twice()
{
  auto [a, b] = co_await when_all(answer(), answer());
  co_return a + b;
}

int main()
{
  thread_executor executor;
  executor.run(false); // pump on an owned thread

  return sync_wait(executor, twice()) == 84 ? 0 : 1;
}
```

More in [puffin/async/README.md](puffin/async/README.md).

### Webkit

```c++
#include <puffin/webkit.hpp>
#include <puffin/webkit/adapter/asio.hpp>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

using server_type = wk::basic_server<wk::middlewares::cookies, wk::middlewares::session>;

// The application, independent of the runtime
void add_routes(server_type& server)
{
  server.get("/users/([0-9]+)", [](auto& ctx) -> pa::async<void> {
    ctx.response().body(co_await load_user(ctx.param(0)), "application/json");
    ctx.session()["last_user"] = std::string(ctx.param(0));
  });
}

// main picks the runtime, asio here
server_type server;
add_routes(server);

::asio::io_context io;
wk::asio::tcp_acceptor acceptor(io, 8080);
pa::co_spawn(pa::asio::executor { io }, server.listen(acceptor));
io.run();
```

More in [puffin/webkit/README.md](puffin/webkit/README.md) and
[samples/webkit_server_sample.cpp](samples/webkit_server_sample.cpp).

### Rest

```c++
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>

namespace rest = puffin::rest;

namespace users {
using namespace puffin::rest;

using get    = endpoint<"users.get",    GET,  "/{id:int}", returns<user>>;
using create = endpoint<"users.create", POST, "/", body<new_user>, returns<user, status::created>>;

using api = rest::api<"/users", get, create>;
}

using v1 = rest::api<"/api/v1", rest::with<rest::security<rest::api_key<"X-Api-Key">>, users::api>>;

// Server: one operator()(Endpoint, args...) per endpoint, checked at compile time
rest::mount<v1>(server, users_service{}, rest::validators(check_api_key));

// Client: the same description
rest::client<v1, wk::any_connector> api(connector, "host", 80); // any_connector: asio, Qt... picked by main
api.credentials<rest::api_key<"X-Api-Key">>(key);
user u = co_await api.call<users::get>(42);
```

More in [puffin/rest/README.md](puffin/rest/README.md) and [samples/rest_sample.cpp](samples/rest_sample.cpp).

### Imdb

```c++
#include <puffin/imdb.hpp>
#include <puffin/imdb/json/nlohmann.hpp>

using namespace puffin::imdb;

in_memory_database<std::string, json> db;
db.insert("cards", R"({ "number": 1, "title": "My Title" })"_json);

auto cards = db.select()
               .from("cards")
               .where(json_search { .key = "title", .text = "My" })
               .order_by(json_sort { .key = "number" })
               .execute();
```

More in [puffin/imdb/README.md](puffin/imdb/README.md).

### Events

```c++
#include <puffin/events/event_bus.hpp>
#include <iostream>

using namespace puffin::events;

struct user_created {};
struct user_deleted {};

using my_events = events<user_created, user_deleted>;

struct audit {
  void handle(const user_created&) { std::cout << "audit: user created\n"; }
};

int main()
{
  audit a;
  event_bus<my_events> bus;

  bus.add_handler<user_created>([](const user_created&) { std::cout << "lambda handler\n"; });
  bus.add_handler<user_created>(&a);    // calls a.handle(event)

  bus.send(user_created {});

  bus.remove_handler<user_created>(&a);
  bus.send(user_created {});            // only the lambda now
}
```

The full example is in [samples/event_bus_sample.cpp](samples/event_bus_sample.cpp).

## Roadmap

What is done and what is next, module by module: [docs/roadmap.md](docs/roadmap.md).

## License

Puffin is released under the [BSD 3-Clause License](LICENSE.md).
