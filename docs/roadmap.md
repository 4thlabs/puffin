# Roadmap

## Common

- [x] Type traits, tuple, string and path literals
- [ ] Unit tests

## Events

- [x] Event bus and handlers
- [ ] Fix unused variable and parameter warnings (`-Wall -Wextra`)

## Ioc

- [x] Container, bindings, allocator
- [ ] Fix unused variable and parameter warnings (`-Wall -Wextra`)

## Maths

- [x] vector3, vector4, matrix4, transform
- [ ] Unit tests for vector4 and transform

## Logging

- [x] Severity levels, colored console output
- [ ] Move to the `puffin` namespace and a `puffin::logging` target
- [ ] Unit tests

## Imdb

- [x] In-memory database, views, requests
- [x] nlohmann JSON serialization

## Async

- [x] Lazy `async<T>` task, executors, `co_spawn`, `sync_wait`
- [x] `when_all`, `schedule_on`, `from_callback`, `try_await`
- [x] Timers: `TimedExecutor` (`post_after`) on every executor, `sleep_for` (replaces the asio and Qt ones)
- [x] asio adapter (standalone and Boost.Asio)
- [x] Qt adapter
- [ ] Awaitable concepts (`Awaitable`, `await_result_t`), today in `webkit/detail/awaitable.hpp`
- [ ] Cancellation (needed for timeouts)
- [ ] `when_any` (needed for timeouts, racing an operation against `sleep_for`)

## Webkit

- [x] Sans-IO HTTP/1.1 parser and serializer (chunked, pipelining, limits)
- [x] Header, method, target and reason validation (no CR, LF or NUL)
- [x] Router with parameters, 405 + Allow, HEAD falls back to GET
- [x] Typed context extended by middlewares
- [x] Async `around` middlewares, matched route reachable from the context
- [x] Client interceptors (`bearer` with refresh, `retry` with backoff and Retry-After, waiting with `async::sleep_for`)
- [x] Cookies middleware
- [x] Session middleware, signed with HMAC-SHA256, expiry checked by the server
- [x] Set-Cookie validation (no attribute injection)
- [x] Coroutine server and client on `puffin::async`
- [x] Server split into facade, connection and shared message reader
- [x] asio adapter, TCP and TLS (OpenSSL)
- [x] Qt adapter, TCP
- [ ] Read, idle and request timeouts: on `async::sleep_for`, waiting for `when_any` and cancellation
- [ ] Connection limit
- [ ] Accept retry with backoff on resource errors (EMFILE)
- [ ] Error callback for failed connections, handler exceptions and oversized sessions
- [ ] Percent-decoded route parameters
- [ ] HEAD in Allow, automatic OPTIONS
- [ ] `Expect: 100-continue`
- [ ] Document server and acceptor lifetime
- [ ] Session key from the OS (`getrandom`, `BCryptGenRandom`) instead of `std::random_device`
- [ ] Concurrent client requests (connection pool)
- [ ] Qt TLS (QSslSocket)
- [ ] Split `parse_start_line()` and `start_body()` of the parser (clang-tidy exemptions today)

## Rest

- [x] Api description as types: named endpoints, typed path parameters, query, header, body, returns
- [x] Nested apis with parameterized prefixes, `with<>` shared parts
- [x] Security schemes: api key (header, query), bearer
- [x] Server: `mount` with compile-time checked services and validators, routes named after endpoints, automatic
  400, 401, 415, 204, error codecs
- [x] Client: `call`, `try_call`, `scope`, `credentials`, webkit interceptors
- [x] Codecs (text, JSON with nlohmann), `param_traits`, `enum_param`, `local_transport`
- [ ] Basic auth scheme
- [ ] OpenAPI document generated from the description

## Tooling

- [x] clang-tidy in CI (function size and complexity) on async, webkit and rest
- [ ] Self-contained headers in every module (CMake `VERIFY_INTERFACE_HEADER_SETS`), then let clang-tidy use the CMake compile commands instead of its own flags
