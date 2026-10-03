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

C++20 coroutines, independent of any event loop (`puffin::async`, header only), with asio and Qt adapters.

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

  return sync_wait(executor, twice()) == 84 ? 0 : 1;
}
```

Full documentation: [puffin/async/README.md](puffin/async/README.md).

### Webkit

HTTP/1.1 server and client on top of `puffin::async` (`puffin::webkit`, header only). The HTTP core has no I/O,
transports come from asio (TCP, TLS) and Qt adapters.

```c++
#include <puffin/webkit.hpp>
#include <puffin/webkit/adapter/asio.hpp>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

wk::basic_server<wk::middlewares::cookies, wk::middlewares::session> server;

server.get("/users/([0-9]+)", [](auto& ctx) -> pa::async<void> {
  ctx.response().body(co_await load_user(ctx.param(0)), "application/json");
  ctx.session()["last_user"] = std::string(ctx.param(0));
});

::asio::io_context io;
wk::asio::tcp_acceptor acceptor(io, 8080);
pa::co_spawn(pa::asio::executor { io }, server.listen(acceptor));
io.run();
```

Full documentation: [puffin/webkit/README.md](puffin/webkit/README.md).

### Imdb

A small in-memory database queried with a fluent request (`puffin::imdb`, header only), with views, indexes and
nlohmann JSON helpers.

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

Full documentation: [puffin/imdb/README.md](puffin/imdb/README.md).

### Ioc
### Maths


[logo]: ./docs/logo.svg 
