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
  `(std::exception_ptr)` or `(std::exception_ptr, T)`. The default (`detached`) silently drops
  exceptions, and a throwing completion calls `std::terminate`.
- `sync_wait`, `when_all` (variadic or `std::vector`), `schedule_on(executor)`, `this_executor`.
- `from_callback<Args...>(initiate)`: awaits any callback based operation, the coroutine resumes on
  its own executor.
- Executors: `thread_executor`, `inline_executor`.

Adapters, built when their dependency is found:

- `puffin::async_asio` (`<puffin/async/adapter/asio.hpp>`, standalone asio or Boost.Asio with
  `PUFFIN_ASYNC_USE_BOOST_ASIO`): `asio::executor { io_context }`, the `asio::use_async` /
  `asio::use_async_tuple` completion tokens, `asio::sleep_for`.

  ```c++
  std::size_t n = co_await socket.async_read_some(buffer, puffin::async::asio::use_async);
  ```

- `puffin::async_qt` (`<puffin/async/adapter/qt.hpp>`, Qt 6): `qt::executor { context_object }`,
  `co_await qt::signal(sender, &Sender::signal)`, `qt::sleep_for`.

### Webkit

HTTP/1.1 server and client on top of `puffin::async` (`puffin::webkit`, header only). The HTTP
core has no I/O, transports come from adapters.

```c++
#include <puffin/webkit.hpp>
#include <puffin/webkit/adapter/asio.hpp>

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
- The session cookie is signed with HMAC-SHA256, set `session::options::secret` to keep sessions
  across restarts and instances (a random key is used otherwise). Its content is readable by the client.
- Header names and values, methods, targets and reason phrases are validated: CR, LF and NUL throw
  `std::invalid_argument`. See [the roadmap](docs/roadmap.md) for what is missing.
- `basic_client<Connector>`: one host, kept alive connection, idempotent requests retried once on
  a stale connection.
- Transports implement the `Stream`, `Acceptor` and `Connector` concepts.

Adapters, built when their dependency is found:

- `puffin::webkit_asio` (`<puffin/webkit/adapter/asio.hpp>`): `asio::tcp_stream`, `asio::tcp_acceptor`,
  `asio::tcp_connector`.
- `puffin::webkit_asio_ssl` (`<puffin/webkit/adapter/asio_ssl.hpp>`, OpenSSL): `asio::tls_acceptor`,
  `asio::tls_connector` (SNI and host name verification).
- `puffin::webkit_qt` (`<puffin/webkit/adapter/qt.hpp>`, Qt 6 Network): `qt::tcp_stream`,
  `qt::tcp_acceptor`, `qt::tcp_connector`, used with `puffin::async::qt::executor`.

### Ioc
### Maths


[logo]: ./docs/logo.svg 
