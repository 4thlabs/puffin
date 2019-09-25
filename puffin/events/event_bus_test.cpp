//  ____         __  __ _       
// |  _ \ _   _ / _|/ _(_)_ __  
// | |_) | | | | |_| |_| | '_  | 
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|          
//
// BSD 3-Clause License

// Copyright (c) 2019, Thomas Gourgues (thomas.gourgues@gmail.com)
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.

// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.

// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <catch2/catch.hpp>
#include "puffin/events/event_bus.hpp"

using namespace pfn::events;

struct my_event {};
struct my_event_2 {};

using my_events = events
<
  my_event,
  my_event_2
>;

class test {
public:
  void handle(const my_event& e) {
    throw "my_event";
  }

  void handle(const my_event_2& e) {
    throw "my_event_2";
  }
};

TEST_CASE("Events are received or not", "[events]") {
  event_bus<my_events> bus;

  bus.add_handler<my_event>([](const my_event& e) {
    throw "my_event";
  });

  SECTION("Events are received", "[Events]") {
    REQUIRE_THROWS_WITH(bus.send(my_event()), Catch::Contains("my_event"));
  }

  SECTION("Events handlers can be cleared") {
    test t;

    bus.add_handler<my_event_2>(&t);

    bus.clear<my_event>();
    REQUIRE_NOTHROW(bus.send(my_event()));
    REQUIRE_THROWS_WITH(bus.send(my_event_2()), Catch::Contains("my_event_2"));
  }

  SECTION("Events handlers can be removed") {
    test t;

    bus.add_handler<my_event_2>(&t);
    REQUIRE_THROWS_WITH(bus.send(my_event_2()), Catch::Contains("my_event_2"));
    
    bus.remove_handler<my_event_2>(&t);
    REQUIRE_NOTHROW(bus.send(my_event_2()));
  }

  SECTION("All events handler can be cleared") {
    test t;

    bus.add_handler<my_event_2>(&t);
    bus.clear_all();

    REQUIRE_NOTHROW(bus.send(my_event()));
    REQUIRE_NOTHROW(bus.send(my_event_2()));
  }

  SECTION("Multiple events can be registered or cleared") {
    test t;

    bus.clear_all();
    register_events<my_event, my_event_2>(bus, &t);

    REQUIRE_THROWS_WITH(bus.send(my_event()), Catch::Contains("my_event"));
    REQUIRE_THROWS_WITH(bus.send(my_event_2()), Catch::Contains("my_event_2"));

    unregister_events<my_event, my_event_2>(bus, &t);

    REQUIRE_NOTHROW(bus.send(my_event()));
    REQUIRE_NOTHROW(bus.send(my_event_2()));
  }
}