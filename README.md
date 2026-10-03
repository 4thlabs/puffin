<p align="center">
  <img src="./docs/logo.svg" height="128">
</p>

# Puffin Cpp

Puffin Cpp is a small c++ library made of simple utilities like event bus, meta template helpers, used to bootstrap projects faster.

### Common
### Events

```c++

#include <puffin/events/event_bus.hpp>
#include <iostream>

using namespace pfn::events;

struct my_event {};
struct my_event_2 {};
struct my_event_3 {};
struct my_event_4 {};

using my_events = events
<
  my_event, 
  my_event_2,
  my_event_3
>;

class my_class {
public:
  void handle(const my_event& e) {
    std::cout << "class: my_event handler" << std::endl;
  }
};

int main(int argc, char** argv) {
  my_class m;
  event_bus<my_events> bus;

  std::function<void(const my_event_2&)> f = [](const my_event_2& e) {
    std::cout << "std::function my_event_2 handler" << std::endl;
  };

  bus.add_handler<my_event>([](const my_event& e) {
    std::cout << "lambda my_event handler" << std::endl;
  });

  bus.add_handler<my_event_2>(f);

  /*bus.add_handler<my_event>(
    std::bind(&my_class::handle, &m, std::placeholders::_1)
  );*/
  
  bus.add_handler<my_event>(&m);

  bus.send(my_event());
  bus.send(my_event_2());
  
  //bus.clear<my_event>();
  bus.remove_handler<my_event>(&m);

  bus.send(my_event());
  bus.send(my_event_2());

  return 0;
}
```

### Async

C++20 coroutines, independent of any event loop (`puffin::async`, header only).

```c++
#include <puffin/async.hpp>

using namespace puffin::async;

async<int> answer() { co_return 42; }

async<int> twice() {
  auto [a, b] = co_await when_all(answer(), answer());
  co_return a + b;
}

int main() {
  thread_executor executor;
  executor.run(false); // pump on an owned thread

  co_spawn(executor, twice(), [](std::exception_ptr ex, int value) { /* ... */ });
  return sync_wait(executor, twice()) == 84 ? 0 : 1;
}
```

- `async<T>`: lazy task, started when awaited, on the executor of the awaiting coroutine.
- `any_executor`: type erased executor (anything with `post(std::coroutine_handle<>)`). An lvalue is
  referenced, an rvalue is owned.
- `co_spawn(executor, task_or_factory, completion)`: starts a detached task. `completion` takes
  `(std::exception_ptr)` or `(std::exception_ptr, T)`.
- `sync_wait`, `when_all` (variadic or `std::vector`), `schedule_on(executor)`, `this_executor`.
- `from_callback<Args...>(initiate)`: awaits any callback based operation, the coroutine resumes on
  its own executor.
- Executors: `thread_executor`, `inline_executor`.

Adapters, built when their dependency is found:

- `puffin::async_asio` (`<puffin/async/asio.hpp>`, standalone asio or Boost.Asio with
  `PUFFIN_ASYNC_USE_BOOST_ASIO`): `asio::executor { io_context }`, the `asio::use_async` /
  `asio::use_async_tuple` completion tokens, `asio::sleep_for`.

  ```c++
  std::size_t n = co_await socket.async_read_some(buffer, puffin::async::asio::use_async);
  ```

- `puffin::async_qt` (`<puffin/async/qt.hpp>`, Qt 6): `qt::executor { context_object }`,
  `co_await qt::signal(sender, &Sender::signal)`, `qt::sleep_for`.

### Webkit

HTTP/1.1 server and client on top of `puffin::async` (`puffin::webkit`, header only). The HTTP
core has no I/O, transports come from adapters.

```c++
#include <puffin/webkit.hpp>
#include <puffin/webkit/asio.hpp>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

wk::basic_server<wk::middlewares::cookies, wk::middlewares::session> server;

server.get("/hello", [](auto& ctx) { ctx.response().body("hello", "text/plain"); });

server.get("/users/([0-9]+)", [](auto& ctx) -> pa::async<void> {
  ctx.response().body(co_await load_user(ctx.param(0)), "application/json");
  ctx.session()["last_user"] = std::string(ctx.param(0));
});

::asio::io_context io;
wk::asio::tcp_acceptor acceptor(io, 8080);
pa::co_spawn(pa::asio::executor { io }, server.listen(acceptor));
io.run();
```

```c++
wk::basic_client client(wk::asio::tcp_connector { io }, "example.com", 80);
wk::response res = co_await client.get("/index.html");
```

- `request` / `response`: plain HTTP messages, shared by the server and the client.
- `request_parser` / `response_parser`: incremental parsers (Content-Length, chunked, pipelining,
  size limits), `serialize()` for the wire format.
- `basic_server<Middlewares...>`: regex routes, handlers returning `void` or `async<void>`, 404,
  405, HEAD, keep-alive. Handlers get a `basic_context<Middlewares...>` that each middleware
  extends with its `data_type` (`ctx.cookies()`, `ctx.set_cookie()`, `ctx.session()`).
- Middlewares define optional `before(ctx)` (returning `false` stops the request) and `after(ctx)`.
- `basic_client<Connector>`: one host, kept alive connection, idempotent requests retried once on
  a stale connection.
- Transports implement the `Stream`, `Acceptor` and `Connector` concepts.

Adapters, built when their dependency is found:

- `puffin::webkit_asio` (`<puffin/webkit/asio.hpp>`): `asio::tcp_stream`, `asio::tcp_acceptor`,
  `asio::tcp_connector`.
- `puffin::webkit_asio_ssl` (`<puffin/webkit/asio_ssl.hpp>`, OpenSSL): `asio::tls_acceptor`,
  `asio::tls_connector` (SNI and host name verification).
- `puffin::webkit_qt` (`<puffin/webkit/qt.hpp>`, Qt 6 Network): `qt::tcp_stream`,
  `qt::tcp_acceptor`, `qt::tcp_connector`, used with `puffin::async::qt::executor`.

### Rest

A DSL describing REST apis once, to serve them and to call them (`puffin::rest`, header only, on
top of `puffin::webkit`). JSON bodies with `puffin::rest_json` (nlohmann::json).

```c++
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>

namespace rest = puffin::rest;

namespace users {
using namespace puffin::rest;

using list   = endpoint<GET,    "/",         query<"limit", std::optional<int>>, returns<std::vector<user>>>;
using get    = endpoint<GET,    "/{id:int}", returns<user>>;
using create = endpoint<POST,   "/",         body<new_user>, returns<user, status::created>>;
using remove = endpoint<DELETE, "/{id:int}">; // 204

using api = rest::api<"/users", list, get, create, remove>;
}

using v1 = rest::api<"/api/v1", auth::api, rest::with<rest::security<rest::api_key<"X-Api-Key">>, users::api>>;
```

Server: one `operator()(Endpoint, args...)` per endpoint, checked at compile time.

```c++
struct users_service {
  async<std::vector<user>> operator()(users::list, std::optional<int> limit);
  async<user> operator()(users::get, int id);  // optionally the context as last argument
  user operator()(users::create, new_user u);
  void operator()(users::remove, int id);
};

rest::mount<v1>(server, auth_service{}, users_service{},
                rest::validators([](const rest::api_key_value& k) { return k.value == key; }),
                rest::interceptors(access_log{}));
```

Client: the same description, over a webkit `Connector` or any `Transport`.

```c++
rest::client<v1, wk::asio::tcp_connector, rest::intercept::api_key, rest::intercept::retry> api(connector, "host", 80);
api.interceptor<rest::intercept::api_key>().key("...");

user u = co_await api.call<users::get>(42);
auto page = co_await api.call<users::list>();           // trailing optionals may be omitted
rest::result<user> r = co_await api.try_call<users::get>(7); // errors as values
```

- Path parameters `{name:type}`: `int`, `int64`, `uint`, `uint64`, `double`, `bool`, `string`
  (default), `path` (rest of the path). Parsed at compile time.
- Endpoints are identified by their type: two with the same method, path and parts in one api are
  rejected at compile time, `name<"users.remove">` tells them apart (and names them for interceptors).
- Parts: `query<"name", T>` (`optional<T>`, `vector<T>`), `header<"Name", T>`, `body<T, Codec>`,
  `returns<T, status, Codec>`, `security<api_key<"Header">, api_key_query<"param">, bearer_auth>`.
- Arguments, server and client alike: path parameters, the endpoint parts in order, then the parts
  inherited from `with<>`. Security schemes are not arguments.
- `api<"/prefix", ...>` nests, prefixes may have parameters (`"/users/{user_id:int}/cards"`),
  `client::scope<SubApi>(args...)` fixes the leading ones.
- Server errors: 400 invalid arguments or body, 415 wrong Content-Type, 401 missing or rejected
  credentials, `http_error` its status as `{"error": "..."}`, other exceptions 500.
- Interceptors, server `(Info, ctx, next_handler)` and client `(Info, request&, next_request)`;
  client ones provided: `intercept::api_key`, `intercept::bearer` (refresh on 401),
  `intercept::retry`. `client::headers()` adds headers to every request.
- `param_traits<T>` converts parameters (`enum_param<E, "a", "b">` for enums), `Codec` encodes
  bodies, `local_transport` calls a server in process.

### Ioc
### Maths


[logo]: ./docs/logo.svg 
