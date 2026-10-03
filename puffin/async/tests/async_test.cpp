//  ____         __  __ _
// |  _ \ _   _ / _|/ _(_)_ __
// | |_) | | | | |_| |_| | '_  |
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|
//
// BSD 3-Clause License

// Copyright (c) 2025, Thomas Gourgues (thomas.gourgues@gmail.com)
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


#include <puffin/async.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace puffin::async;

namespace {

struct no_default {
  explicit no_default(int v)
      : value(v)
  {}

  int value;
};

async<int> answer() { co_return 42; }

async<void> nothing() { co_return; }

async<std::unique_ptr<int>> move_only() { co_return std::make_unique<int>(7); }

async<no_default> not_default_constructible() { co_return no_default { 3 }; }

async<int> throwing()
{
  throw std::runtime_error("boom");
  co_return 0;
}

async<int> nested()
{
  int a = co_await answer();
  auto p = co_await move_only();
  auto n = co_await not_default_constructible();
  co_await nothing();
  co_return a + *p + n.value;
}

async<std::string> catching()
{
  try {
    co_await throwing();
  } catch (const std::runtime_error& e) {
    co_return e.what();
  }

  co_return "";
}

// Simulates a callback based API completing on a foreign thread
void async_add(int a, int b, std::function<void(int)> callback)
{
  std::thread([=] { callback(a + b); }).detach();
}

}

TEST_CASE("async", "[async]")
{
  SECTION("sync_wait runs a task inline")
  {
    REQUIRE(sync_wait(answer()) == 42);
    REQUIRE(sync_wait(nested()) == 52);
    REQUIRE(sync_wait(catching()) == "boom");
    REQUIRE_THROWS_AS(sync_wait(throwing()), std::runtime_error);
  }

  SECTION("tasks are lazy")
  {
    bool started = false;
    auto make = [&]() -> async<void> {
      started = true;
      co_return;
    };

    auto task = make();
    REQUIRE_FALSE(started);
    sync_wait(std::move(task));
    REQUIRE(started);
  }

  SECTION("co_spawn on a thread executor")
  {
    thread_executor executor;
    executor.run(false);

    std::promise<int> result;
    std::atomic<bool> failed = false;
    std::atomic<bool> on_executor = false;

    co_spawn(executor, nested(), [&](std::exception_ptr ex, int value) {
      failed = ex != nullptr;
      on_executor = executor.running_in_this_thread();
      result.set_value(value);
    });

    REQUIRE(result.get_future().get() == 52);
    REQUIRE_FALSE(failed);
    REQUIRE(on_executor);
  }

  SECTION("co_spawn reports exceptions")
  {
    thread_executor executor;
    executor.run(false);

    std::promise<std::exception_ptr> result;
    co_spawn(executor, throwing(), [&](std::exception_ptr ex) { result.set_value(ex); });

    REQUIRE_THROWS_AS(std::rethrow_exception(result.get_future().get()), std::runtime_error);
  }

  SECTION("co_spawn keeps the factory alive")
  {
    thread_executor executor;
    executor.run(false);

    std::promise<int> result;
    auto value = std::make_shared<int>(5);

    co_spawn(
        executor,
        [value]() -> async<int> { co_return *value * 2; },
        [&](std::exception_ptr, int v) { result.set_value(v); });

    REQUIRE(result.get_future().get() == 10);
  }

  SECTION("thread executor resumes in posting order")
  {
    thread_executor executor;
    std::vector<int> order;

    for (int i = 0; i < 5; ++i)
      co_spawn(executor, [&order, i]() -> async<void> {
        order.push_back(i);
        co_return;
      });

    co_spawn(executor, [&]() -> async<void> {
      executor.stop();
      co_return;
    });

    executor.run();
    REQUIRE(order == std::vector<int> { 0, 1, 2, 3, 4 });
  }

  SECTION("schedule_on and this_executor")
  {
    thread_executor a;
    thread_executor b;
    a.run(false);
    b.run(false);

    auto inner = [&]() -> async<bool> {
      co_await schedule_on(b);
      co_return b.running_in_this_thread();
    };

    auto outer = [&]() -> async<bool> {
      co_await schedule_on(a);
      any_executor current = co_await this_executor;
      bool on_a = a.running_in_this_thread() && current == any_executor(a);
      bool inner_on_b = co_await inner();
      // Back on a once inner is done
      co_return on_a && inner_on_b && a.running_in_this_thread();
    };

    REQUIRE(sync_wait(outer()));
  }

  SECTION("when_all")
  {
    thread_executor executor;
    executor.run(false);

    auto all = [&]() -> async<int> {
      auto [a, n, s] = co_await when_all(answer(), nothing(), catching());
      (void)n;

      std::vector<async<int>> tasks;
      for (int i = 0; i < 10; ++i)
        tasks.push_back([](int i) -> async<int> { co_return i; }(i));

      auto values = co_await when_all(std::move(tasks));

      int sum = 0;
      for (int v : values)
        sum += v;

      co_return a + sum + static_cast<int>(s.size());
    };

    REQUIRE(sync_wait(executor, all()) == 42 + 45 + 4);
  }

  SECTION("when_all rethrows")
  {
    REQUIRE_THROWS_AS(sync_wait(when_all(answer(), throwing())), std::runtime_error);
  }

  SECTION("when_all across threads")
  {
    thread_executor executor;
    executor.run(false);

    auto add = [](int a, int b) -> async<int> {
      co_return co_await from_callback<int>([=](auto cb) { async_add(a, b, cb); });
    };

    auto all = [&]() -> async<bool> {
      std::vector<async<int>> tasks;
      for (int i = 0; i < 50; ++i)
        tasks.push_back(add(i, i));

      auto values = co_await when_all(std::move(tasks));
      bool ok = executor.running_in_this_thread();

      for (int i = 0; i < 50; ++i)
        ok = ok && values[i] == 2 * i;

      co_return ok;
    };

    REQUIRE(sync_wait(executor, all()));
  }

  SECTION("from_callback")
  {
    thread_executor executor;
    executor.run(false);

    auto task = [&]() -> async<bool> {
      // Completion from a foreign thread, resumed on the executor
      int sum = co_await from_callback<int>([](auto cb) { async_add(1, 2, cb); });
      bool on_executor = executor.running_in_this_thread();

      // Completion inline, from the initiating function
      auto [x, y] = co_await from_callback<int, std::string>([](auto cb) { cb(4, "four"); });

      // No arguments
      co_await from_callback<>([](auto cb) { cb(); });

      co_return sum == 3 && on_executor && x == 4 && y == "four";
    };

    REQUIRE(sync_wait(executor, task()));
  }

  SECTION("any_executor")
  {
    thread_executor t;
    any_executor empty;
    any_executor ref(t);
    any_executor owned(inline_executor {});

    REQUIRE_FALSE(empty);
    REQUIRE(ref);
    REQUIRE(ref == any_executor(t));
    REQUIRE(owned == any_executor(inline_executor {}));
    REQUIRE_FALSE(ref == owned);
  }
}
