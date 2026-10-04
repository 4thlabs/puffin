# puffin::webkit

An HTTP/1.1 server and client written with [`puffin::async`](../async/README.md) coroutines. The HTTP core
(messages, parser, serializer, router, middlewares) does no I/O: sockets come from small transport adapters for
asio and Qt, or from your own. Header only, C++20.

- Target: `puffin::webkit` (depends on `puffin::async`)
- Header: `<puffin/webkit.hpp>`
- Transports, built when their dependency is found: `puffin::webkit_asio`, `puffin::webkit_asio_ssl`,
  `puffin::webkit_qt`

```cmake
target_link_libraries(my_app PRIVATE puffin::webkit_asio)
```

## Quick start

```c++
#include <puffin/webkit.hpp>
#include <puffin/webkit/adapter/asio.hpp>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

int main()
{
  ::asio::io_context io;
  pa::asio::executor executor { io };

  wk::basic_server<wk::middlewares::cookies, wk::middlewares::session> server;

  server.get("/", [](auto& ctx) { ctx.response().body("Hello\n", "text/plain"); });

  server.get("/visits", [](auto& ctx) {
    auto& visits = ctx.session()["visits"];
    visits = std::to_string(visits.empty() ? 1 : std::stoi(visits) + 1);
    ctx.response().body("Visits: " + visits + "\n", "text/plain");
  });

  server.get("/wait/([0-9]+)", [executor](auto& ctx) -> pa::async<void> {
    co_await pa::asio::sleep_for(executor, std::chrono::milliseconds(std::stoi(std::string(ctx.param(0)))));
    ctx.response().body("Waited\n", "text/plain");
  });

  wk::asio::tcp_acceptor acceptor(io, 8080);
  pa::co_spawn(executor, server.listen(acceptor));
  io.run();
}
```

A complete version, with clean shutdown on Ctrl+C, is in [`samples/webkit_server_sample.cpp`](../../samples/webkit_server_sample.cpp).

## Architecture

```
  basic_server / basic_client          coroutines, one per connection
          │
  request_parser · router · middlewares · serialize()     sans-IO HTTP core
          │
  Stream / Acceptor / Connector concepts
          │
  asio (TCP, TLS) · Qt (TCP) · your transport
```

The server and client only see the three transport concepts, so the same HTTP code runs on asio, on the Qt event
loop, or on an in-memory pipe in tests. Coroutines run on the executor of the coroutine that called `listen()` or
the client method, see [where a coroutine runs](../async/README.md#where-a-coroutine-runs).

## Server

### Routes

```c++
wk::basic_server<> server;                       // no middleware

server.get("/users", list_users);
server.post("/users", create_user);
server.get("/users/([0-9]+)", show_user);         // ctx.param(0) is the id
server.route("OPTIONS", "/users", options);       // any method
```

- `get`, `post`, `put`, `patch`, `del` and `route(method, pattern, handler, name)`. The optional name is
  reachable by middlewares with `ctx.route()->name`.
- Patterns are `std::regex` matched against the **whole** path (query excluded). Capture groups become the route
  parameters, read with `ctx.param(i)` (empty if missing) or `ctx.params()`.
- Routes are tried in insertion order, the first match wins.
- No match gives a `404`. A path matching only with other methods gives a `405` with an `Allow` header.
- `HEAD` falls back to the `GET` route, the body is dropped and the headers kept.
- Paths are matched raw: percent-encoded characters are not decoded first.

Routes and middlewares must be set up before serving.

### Handlers

A handler takes the context and returns either nothing or `async<void>`:

```c++
server.get("/hello", [](auto& ctx) {
  ctx.response().body("hello", "text/plain");
});

server.get("/users/([0-9]+)", [](auto& ctx) -> pa::async<void> {
  ctx.response().body(co_await load_user(ctx.param(0)), "application/json");
});
```

Handlers are stored in the server, so their captures live as long as it does. The response starts as `200 OK`
with an empty body. A handler that throws produces a `500 Internal Server Error`.

### Context

`basic_context<Middlewares...>` is built for each request:

| Member | Description |
| --- | --- |
| `request()` | The parsed `request` (method, target, `path()`, `query()`, headers, body). |
| `response()` | The `response` to send. |
| `param(i)`, `params()` | Route parameters. |
| `route()` | The matched route (`method`, `pattern`, `name`), `nullptr` for a 404 or 405. Known before the middlewares run. |
| `data<M>()` | Data of middleware `M`, when two middlewares expose the same names. |

Each middleware with a nested `data_type` adds it as a base of the context, so its members are reachable directly:
`ctx.cookies()`, `ctx.session()`... This is resolved at compile time from the middleware list, a handler of a
server without the session middleware simply has no `ctx.session()`.

### Middlewares

A middleware is any class with, all optional:

- a nested `data_type`, added to the context;
- `before(ctx)`, called in order before the handler. Returning `false` stops the request: the next middlewares and
  the handler are skipped and the response is sent as is;
- `async<void> around(ctx, wk::next_handler next)`, called in order once every `before` ran, around the handler.
  It may answer the request itself, or `co_await next()` and then look at the response;
- `after(ctx)`, called in reverse order after the handler, only for the middlewares whose `before` ran.

```c++
struct api_key {
  template<typename Context>
  bool before(Context& ctx)
  {
    if (ctx.request().headers().get("X-Api-Key") == key)
      return true;
    ctx.response().status(wk::status::unauthorized);
    return false;
  }

  std::string key;
};

wk::basic_server<api_key> server({}, api_key { .key = "secret" });
server.middleware<api_key>().key = "other";    // instances stay reachable
```

An asynchronous middleware uses `around`:

```c++
struct access_log {
  template<typename Context>
  pa::async<void> around(Context& ctx, wk::next_handler next)
  {
    auto start = std::chrono::steady_clock::now();
    co_await next();
    log(ctx.route() ? ctx.route()->name : "-", ctx.response().status_code(), std::chrono::steady_clock::now() - start);
  }
};
```

### Cookies

`wk::middlewares::cookies` parses the `Cookie` headers and writes the cookies set by the handler:

```c++
server.get("/", [](auto& ctx) {
  std::optional<std::string_view> theme = ctx.cookie("theme");   // ctx.cookies() for all of them

  wk::cookie c("theme", "dark");
  c.max_age = std::chrono::days(30);
  c.same_site = wk::cookie::same_site_policy::lax;
  ctx.set_cookie(c);
});
```

`cookie` has `name`, `value`, `path` (`/`), `domain`, `max_age`, `same_site`, `secure` and `http_only`. Invalid
names, values, paths or domains throw `std::invalid_argument` when the `Set-Cookie` header is built, which turns
into a `500`.

### Sessions

`wk::middlewares::session` keeps a `std::map<std::string, std::string>` in a signed cookie, reachable with
`ctx.session()`. It is restored from the request and written back only when modified; emptying it expires the
cookie.

```c++
wk::middlewares::session::options options;
options.secret = load_secret();              // 32 bytes or more
options.max_age = std::chrono::hours(8);
options.secure = true;

wk::basic_server<wk::middlewares::session> server({}, wk::middlewares::session(options));
```

| Option | Default | Description |
| --- | --- | --- |
| `cookie_name` | `puffin_session` | |
| `path` | `/` | |
| `max_age` | 24 h | The cookie expires `max_age` after the last change, checked by the server too. |
| `secure`, `http_only` | `false`, `true` | Cookie attributes. |
| `secret` | random | HMAC-SHA256 key. Without it a random key is generated: sessions are lost on restart and not shared between instances. |
| `max_cookie_size` | 4096 | Larger sessions fail the response with a `500`. |

The cookie value is `expiry.payload.signature`, signed over the cookie name, expiry and payload. A cookie with a
bad signature, expired, or copied under another name is ignored. The content is **signed, not encrypted**: the
client can read it but not change it, so do not store secrets in it.

### Serving

```c++
pa::co_spawn(executor, server.listen(acceptor), [](std::exception_ptr e) { /* acceptor error */ });
```

- `listen(acceptor)` accepts connections until the acceptor is closed, then returns. Each connection is served by
  its own task on the executor of the calling coroutine.
- `serve(stream)` serves one connection: keep-alive, pipelining, `Connection: close`, HTTP/1.0. Transport errors
  end the connection silently. Invalid requests get a `400`, `414`, `413` or `431`, then the connection is closed.
- `handle(request, response)` runs middlewares and routes without any I/O, handy in unit tests.

`server_options` sets the parser limits (`max_header_size` 64 KiB, `max_body_size` 8 MiB, `max_target_size`
4 KiB) and the read buffer size.

The server, its acceptor and the executor must outlive every connection. When connections run on several threads,
handlers and middlewares must be thread safe.

## Client

```c++
wk::basic_client client(wk::asio::tcp_connector { io }, "example.com", 80);

wk::response res = co_await client.get("/index.html");
if (res.status_code() == 200)
  std::cout << res.body();

co_await client.post("/users", R"({"name":"puffin"})", "application/json");
```

- One client talks to one host. The connection opens on the first request and is kept alive.
- `get`, `post`, `put`, `del`, or `request(wk::request)` for full control. `Host` and `Connection` are filled when
  missing.
- An idempotent request failing on a reused connection (closed by the server meanwhile) is retried once on a new one.
- Errors are thrown: transport errors from the adapter, `wk::protocol_error` for an invalid response,
  `wk::connection_closed` when the server closes before answering.
- One request at a time (a second concurrent one throws `std::logic_error`). Use one client per concurrent task.
- `client_options`: parser limits, read buffer size, `keep_alive`.

### Interceptors

`basic_client<Connector, Interceptors...>` runs interceptors around each request. An interceptor is a class with
`async<wk::response> operator()(wk::request&, wk::next_request next)`: it may change the request, answer it itself,
or `co_await next(req)`, several times if needed.

```c++
wk::basic_client<wk::asio::tcp_connector, wk::interceptors::bearer, wk::interceptors::retry> client(connector, "example.com", 80);

client.interceptor<wk::interceptors::bearer>().token(token);
client.interceptor<wk::interceptors::bearer>().on_refresh([]() -> pa::async<std::string> { co_return co_await login(); });
```

- `interceptors::bearer` sends `Authorization: Bearer <token>`. With `on_refresh`, a `401` refreshes the token and
  sends the request again, once.
- `interceptors::retry` sends idempotent requests again when the transport fails or the server answers `502`, `503`
  or `504`, 3 attempts by default (`attempts(n)`). A malformed response is not retried. Attempts are spaced by an
  exponential backoff (`backoff(first, max)`, 100 ms doubling up to 5 s) or by the `Retry-After` of the response
  (seconds, capped at the maximum). Waiting depends on the event loop, so it is given with `sleep(f)`, for instance
  `[ex](auto d) { return pa::asio::sleep_for(ex, d); }`; without it attempts follow each other immediately.

## HTTP messages

`request` and `response` are plain HTTP messages shared by the server and the client.

```c++
wk::request req("GET", "/search?q=puffin");
req.headers().set("Accept", "application/json");
req.path();          // "/search"
req.query();         // multimap { "q": "puffin" }, percent-decoded

wk::response res(wk::status::created);
res.body(R"({"id":1})", "application/json");   // also sets Content-Type
res.headers().add("Location", "/users/1");
```

`headers` keeps fields in order with case-insensitive lookup: `add`, `set`, `erase`, `get`, `get_all`, `contains`,
`has_token("Connection", "close")`. Header names and values, methods, targets and reason phrases are validated:
CR, LF, NUL and other controls throw `std::invalid_argument`, so user input can't inject headers.

### Parser and serializer

`request_parser` and `response_parser` are incremental and can be used on their own:

```c++
wk::request_parser parser;                         // parser_limits as argument
auto result = parser.feed(bytes);                  // as many times as needed
if (result.status == wk::parse_status::done) {
  wk::request req = parser.release();              // bytes after result.consumed belong to the next message
} else if (result.status == wk::parse_status::error) {
  auto why = parser.error();                       // wk::parse_error
}

std::string wire = wk::serialize(res);
```

They handle `Content-Length`, chunked bodies with trailers, pipelining and size limits, reject bare LF line
endings and conflicting framing headers. `response_parser::skip_body()` handles responses to `HEAD`, `finish()`
ends a body delimited by the end of the connection.

## Transports

### Concepts

| Concept | Requirements |
| --- | --- |
| `Stream` | `co_await read_some(std::span<char>)` returns the bytes read (0 at end of stream), `co_await write(std::span<const char>)` writes everything, `close()`. Errors are thrown. |
| `Acceptor` | `co_await accept()` returns a `Stream`, `close()`, `is_open()`. After `close()`, pending and later `accept()` fail. |
| `Connector` | `co_await connect(host, port)` returns a connected `Stream`, name resolution and TLS handshake included. |

Any type satisfying them works with the server and client, `tests/memory_transport.hpp` is a small in-memory
example.

### asio

`puffin::webkit_asio`, header `<puffin/webkit/adapter/asio.hpp>`, namespace `puffin::webkit::asio`. Use it with
`puffin::async::asio::executor`.

- `tcp_acceptor(io, port, address = "0.0.0.0")`: port 0 picks a free port, read it back with `port()`.
- `tcp_connector { io }`: resolves the host and connects.
- `tcp_stream`: the connected socket, `socket()` gives the underlying asio socket.

### asio TLS

`puffin::webkit_asio_ssl` (OpenSSL), header `<puffin/webkit/adapter/asio_ssl.hpp>`. Disable it with
`-DPUFFIN_WEBKIT_WITH_SSL=OFF`.

```c++
::asio::ssl::context tls(::asio::ssl::context::tls_server);
tls.use_certificate_chain_file("cert.pem");
tls.use_private_key_file("key.pem", ::asio::ssl::context::pem);

wk::asio::tls_acceptor acceptor(io, 8443, tls);
pa::co_spawn(executor, server.listen(acceptor));
```

```c++
::asio::ssl::context tls(::asio::ssl::context::tls_client);
tls.set_default_verify_paths();

wk::basic_client client(wk::asio::tls_connector(io, tls), "example.com", 443);
```

The client sends SNI and verifies the certificate and host name (`verify_peer(false)` disables it, for tests only).
The server handshake runs on the connection's first read, so a slow client does not block `accept()`. The SSL
context must outlive the acceptor, connector and their streams.

### Qt

`puffin::webkit_qt` (Qt 6 Network), header `<puffin/webkit/adapter/qt.hpp>`, namespace `puffin::webkit::qt`. Use it
with `puffin::async::qt::executor`, from a thread running a Qt event loop.

```c++
wk::qt::tcp_acceptor acceptor(8080);
pa::co_spawn(pa::qt::executor {}, server.listen(acceptor));
app.exec();

wk::basic_client client(wk::qt::tcp_connector {}, "example.com", 80);
```

No TLS on Qt yet.

## Limitations

No timeouts, connection limit, async middlewares, connection pool or Qt TLS yet. The full list is in the
[roadmap](../../docs/roadmap.md#webkit).
