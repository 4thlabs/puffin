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

### Ioc
### Maths


[logo]: ./docs/logo.svg 
