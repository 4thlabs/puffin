# puffin::async

C++20 coroutines that do not depend on any event loop. Tasks are lazy, run on an *executor* you choose (a thread,
an asio `io_context`, the Qt event loop, ...) and always come back to their executor after an await. Header only.

- Target: `puffin::async`
- Header: `<puffin/async.hpp>`
- Adapters, built when their dependency is found: `puffin::async_asio`, `puffin::async_qt`

```cmake
target_link_libraries(my_app PRIVATE puffin::async)
```

## Quick start

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
  executor.run(false);                     // pump on a thread owned by the executor

  co_spawn(executor, twice(), [](std::exception_ptr ex, int value) {
    // runs on the executor thread once twice() is done
  });

  return sync_wait(executor, twice()) == 84 ? 0 : 1;
}
```

## Concepts

### Tasks: `async<T>`

A function returning `async<T>` (`async<>` is `async<void>`) is a coroutine. Calling it creates the coroutine
frame but runs nothing: the body starts when the task is `co_await`ed or handed to `co_spawn`, `sync_wait` or
`when_all`. The result, or the exception the body threw, is delivered by `co_await`.

- `async<T>` is move only. Destroying it destroys the frame, so only destroy a task that has not started or that
  has finished (awaiting it guarantees the latter).
- `async<T&>` is not supported, return a pointer or a `std::reference_wrapper`.
- `valid()` is `false` for a default constructed or moved from task.

### Executors

An executor is anything with a thread safe `post(std::coroutine_handle<>)` that resumes the handle later on its own
context (concept `Executor`). `any_executor` type-erases them:

- built from an **lvalue**, it *references* the executor, which must outlive every coroutine using it
  (`thread_executor`, which is not copyable, is used this way);
- built from an **rvalue**, it *owns* a copy (lightweight handles such as `asio::executor` or `qt::executor`).

Two executors compare equal when they are the same object or when the underlying type is equality comparable and
the values compare equal.

An executor with a timer also has a thread safe `post_after(std::coroutine_handle<>, std::chrono::nanoseconds)`
(concept `TimedExecutor`), used by `sleep_for`. Every built-in executor has one.

### Where a coroutine runs

Every `async` coroutine knows its executor:

1. A spawned task runs on the executor given to `co_spawn` / `sync_wait`.
2. An awaited task inherits the executor of the coroutine awaiting it.
3. When a task finishes, its awaiter resumes directly if both share the executor, otherwise the awaiter is posted
   back to its own executor. A coroutine therefore never silently changes thread.
4. `co_await schedule_on(other)` moves the current coroutine (and the tasks it awaits from then on) to `other`.

A coroutine without executor (`sync_wait(task)` with no executor, or an `inline_executor`) simply continues on
whatever thread resumes it.

## API

| Name | Header | Description |
| --- | --- | --- |
| `async<T>` | `async.hpp` | Lazy task. |
| `co_spawn(executor, task, completion)` | `co_spawn.hpp` | Starts a detached task. |
| `co_spawn(executor, factory, completion)` | `co_spawn.hpp` | Same, the task is created by `factory()`, which is kept alive until the end. |
| `sync_wait([executor,] task)` | `sync_wait.hpp` | Blocks the calling thread until the task is done, returns its value or rethrows. |
| `when_all(tasks...)` / `when_all(std::vector<async<T>>)` | `when_all.hpp` | Runs tasks concurrently, returns a tuple (or a vector) of results. |
| `try_await(task)` | `try_await.hpp` | Awaits a task and returns its value or exception (`outcome<T>`), to handle a failure with `co_await`. |
| `schedule_on(executor)` | `schedule.hpp` | Awaitable moving the coroutine to another executor. |
| `this_executor` | `schedule.hpp` | `any_executor ex = co_await this_executor;` |
| `sleep_for(duration)` | `sleep.hpp` | `co_await sleep_for(200ms);` resumes on the same executor once the delay elapsed (`TimedExecutor`). Without an executor, blocks the calling thread. |
| `from_callback<Args...>(initiate)` | `from_callback.hpp` | Awaits any callback based operation. |
| `executor_of(handle)` | `executor.hpp` | Executor of a coroutine handle, for custom awaitables. |
| `thread_executor`, `inline_executor` | `impl/thread_executor.hpp` | Built-in executors. |

### `co_spawn`

```c++
co_spawn(executor, task());                                    // fire and forget
co_spawn(executor, task(), [](std::exception_ptr ex) { ... }); // async<void>, or ignore the value
co_spawn(executor, compute(), [](std::exception_ptr ex, int value) { ... });
```

- The task is started from a call posted to the executor (inline if the executor is empty), and the completion runs
  on the executor once it is done.
- The completion takes `(std::exception_ptr)` or `(std::exception_ptr, T)`. In the second form `T` must be default
  constructible: a default value is passed when the task threw.
- The default completion, `detached`, silently drops exceptions. A throwing completion calls `std::terminate`.

**Prefer the factory form with capturing lambdas.** A lambda coroutine reads its captures from the lambda object,
not from the frame, so the lambda must outlive the coroutine:

```c++
// Wrong: the lambda is a temporary, `name` dangles as soon as the coroutine suspends
co_spawn(executor, [name]() -> async<> { co_await greet(name); }());

// Right: co_spawn keeps the lambda alive until the task is done
co_spawn(executor, [name]() -> async<> { co_await greet(name); });
```

### `sync_wait`

Bridges synchronous code (a `main`, a test) to coroutines. With an executor, the task starts on it; without, it
starts inline and continues wherever its awaits resume it. Never call `sync_wait` from a thread the task needs to
make progress, such as the thread pumping its executor: it would deadlock.

### `when_all`

```c++
auto [user, posts] = co_await when_all(load_user(id), load_posts(id));
std::vector<int> sizes = co_await when_all(std::move(tasks));   // std::vector<async<int>>
```

- Tasks are started in order on the awaiting coroutine's executor. They overlap at their suspension points; they
  only run in parallel if they move to other executors.
- `void` results become `std::monostate`.
- If tasks fail, the first exception in argument order is rethrown once *all* of them are done.

### `try_await`

`co_await` is not allowed inside a `catch` block. When a failure must be handled by awaiting something (retry,
reconnect, cleanup), capture the outcome first and branch outside of the handler:

```c++
auto first = co_await try_await(exchange(request));

if (!first) {
  co_await reconnect();
  co_return co_await exchange(request);   // or first.rethrow()
}

co_return std::move(*first.value);
```

`void` tasks give an `outcome<std::monostate>`.

### `from_callback`

Turns any callback API into an awaitable. `Args...` are the callback parameters, `co_await` returns nothing, the
value, or a `std::tuple<Args...>`:

```c++
auto [ec, n] = co_await from_callback<std::error_code, std::size_t>([&](auto callback) {
  socket.async_read_some(buffer, callback);
});
```

The callback is copyable and must be called exactly once, from any thread, possibly inline from `initiate`. The
coroutine is resumed on its own executor.

### `thread_executor`

A single thread executor resuming coroutines in posting order.

| Member | Description |
| --- | --- |
| `run()` | Pumps on the calling thread until `stop()`. |
| `run(false)` | Pumps on a thread owned by the executor. |
| `stop()` | Asks the loop to return once the current coroutine suspends. |
| `restart()` | Allows `run()` again after `stop()`. |
| `wait()` | Stops and joins the owned thread (also done by the destructor). |
| `schedule()` | `co_await executor.schedule();` moves the coroutine to this executor. |
| `post_after(h, delay)` | Resumes `h` once `delay` elapsed, in deadline order. |
| `running_in_this_thread()` | `true` on the pumping thread. |

Coroutines still queued or waiting for their delay when the loop stops are not resumed. `inline_executor` resumes in
`post()` directly (and blocks in `post_after()`) and is mostly useful in tests.

### Writing an awaitable

A custom awaitable that completes from another thread should resume the coroutine on its executor:

```c++
template<typename P>
void await_suspend(std::coroutine_handle<P> h)
{
  any_executor ex = executor_of(h);   // empty if the coroutine is not executor aware
  start_operation([h, ex] { if (ex) ex.post(h); else h.resume(); });
}
```

`from_callback` already does this and is usually enough.

## asio adapter

`puffin::async_asio`, header `<puffin/async/adapter/asio.hpp>`, namespace `puffin::async::asio`. Uses standalone
asio, or Boost.Asio when configured with `-DPUFFIN_ASYNC_USE_BOOST_ASIO=ON`. Disable it with
`-DPUFFIN_ASYNC_WITH_ASIO=OFF`.

```c++
#include <puffin/async/adapter/asio.hpp>

namespace pa = puffin::async;

pa::async<> echo(::asio::ip::tcp::socket socket)
{
  char data[1024];
  for (;;) {
    std::size_t n = co_await socket.async_read_some(::asio::buffer(data), pa::asio::use_async);
    co_await ::asio::async_write(socket, ::asio::buffer(data, n), pa::asio::use_async);
  }
}

::asio::io_context io;
pa::co_spawn(pa::asio::executor { io }, echo(std::move(socket)));
io.run();
```

- `asio::executor { io_context }` (or any `any_io_executor`): resumes coroutines through `asio::post`.
- `asio::use_async`: completion token making any asio operation awaitable. A leading `error_code` (or
  `exception_ptr`) is thrown as `system_error` when set, and `co_await` returns the remaining arguments.
- `asio::use_async_tuple`: throws nothing, `co_await` returns all the arguments as a tuple
  (`auto [ec, n] = co_await ...`).
- `asio::sleep_for(executor, duration)`: `async<void>` waiting on a `steady_timer`.

Keep sockets and timers alive in the awaiting coroutine frame: the operation must not outlive it.

## Qt adapter

`puffin::async_qt`, header `<puffin/async/adapter/qt.hpp>`, namespace `puffin::async::qt`, needs Qt 6 Core.
Disable it with `-DPUFFIN_ASYNC_WITH_QT=OFF`.

```c++
#include <puffin/async/adapter/qt.hpp>

namespace pa = puffin::async;

pa::async<> wait_for_click(QPushButton* button)
{
  co_await pa::qt::signal(button, &QPushButton::clicked);   // returns the signal arguments
  co_await pa::qt::sleep_for(std::chrono::milliseconds(200));
}

pa::co_spawn(pa::qt::executor {}, wait_for_click(button));
app.exec();
```

- `qt::executor { context }`: resumes coroutines from the event loop of the thread `context` lives in (the
  application object by default). Coroutines posted after `context` is destroyed are never resumed.
- `qt::signal(sender, &Sender::signal)`: awaits the next emission and returns its arguments (nothing, the value,
  or a tuple), without the private signal tag. Never resumes if the sender is destroyed first.
- `qt::sleep_for(duration, context)`: single shot timer in the thread of `context`.

## Limitations

- No cancellation and no `when_any` yet, so no timeouts: `sleep_for` is the timer they will build on.
- The awaitable concepts (`Awaitable`, `await_result_t`) still live in webkit.

See the [roadmap](../../docs/roadmap.md) and the tests in [`tests/`](tests).
