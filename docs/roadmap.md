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
- [x] `when_all`, `schedule_on`, `from_callback`
- [x] asio adapter (standalone and Boost.Asio)
- [x] Qt adapter
- [ ] Awaitable concepts (`Awaitable`, `await_result_t`), today in `webkit/detail/awaitable.hpp`
- [ ] Cancellation
- [ ] `when_any`

## Webkit

- [x] Sans-IO HTTP/1.1 parser and serializer (chunked, pipelining, limits)
- [x] Header, method, target and reason validation (no CR, LF or NUL)
- [x] Router with parameters, 405 + Allow, HEAD falls back to GET
- [x] Typed context extended by middlewares
- [x] Cookies middleware
- [x] Session middleware, signed with HMAC-SHA256
- [x] Coroutine server and client on `puffin::async`
- [x] asio adapter, TCP and TLS (OpenSSL)
- [x] Qt adapter, TCP
- [ ] Read, idle and request timeouts
- [ ] Connection limit
- [ ] Accept retry with backoff on resource errors (EMFILE)
- [ ] Error callback for failed connections
- [ ] Percent-decoded route parameters
- [ ] HEAD in Allow, automatic OPTIONS
- [ ] `Expect: 100-continue`
- [ ] Document server and acceptor lifetime
- [ ] Session expiry inside the signed payload
- [ ] Async middlewares
- [ ] Concurrent client requests (connection pool)
- [ ] Qt TLS (QSslSocket)
- [ ] REST DSL (`puffin::rest`), in progress
