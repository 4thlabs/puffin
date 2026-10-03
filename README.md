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

### Ioc
### Maths


[logo]: ./docs/logo.svg 
