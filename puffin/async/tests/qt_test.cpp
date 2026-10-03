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
#include <puffin/async/qt.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <string>

using namespace puffin::async;
using namespace std::chrono_literals;

namespace {

QCoreApplication& application()
{
  // Other tests in the same binary may already have created the application
  if (auto* app = QCoreApplication::instance())
    return *app;

  static int argc = 1;
  static char name[] = "main_test";
  static char* argv[] = { name, nullptr };
  static QCoreApplication app(argc, argv);
  return app;
}

}

TEST_CASE("qt adapter", "[async][qt]")
{
  auto& app = application();

  SECTION("co_spawn on the event loop")
  {
    bool on_main_thread = false;
    int result = 0;

    co_spawn(
        qt::executor {},
        [&]() -> async<int> {
          on_main_thread = QThread::currentThread() == app.thread();
          co_return 4;
        },
        [&](std::exception_ptr, int value) {
          result = value;
          app.quit();
        });

    app.exec();
    REQUIRE(on_main_thread);
    REQUIRE(result == 4);
  }

  SECTION("sleep_for")
  {
    auto start = std::chrono::steady_clock::now();
    bool done = false;

    co_spawn(qt::executor {}, [&]() -> async<void> {
      co_await qt::sleep_for(20ms);
      done = true;
      app.quit();
    });

    app.exec();
    REQUIRE(done);
    REQUIRE(std::chrono::steady_clock::now() - start >= 20ms);
  }

  SECTION("signals")
  {
    QString name;
    bool destroyed = false;

    co_spawn(qt::executor {}, [&]() -> async<void> {
      auto* object = new QObject();

      // Private signal, the QPrivateSignal tag is dropped
      QTimer::singleShot(0, object, [object] { object->setObjectName("puffin"); });
      name = co_await qt::signal(object, &QObject::objectNameChanged);

      QTimer::singleShot(0, object, [object] { delete object; });
      destroyed = co_await qt::signal(object, &QObject::destroyed) == object;

      app.quit();
    });

    app.exec();
    REQUIRE(name == "puffin");
    REQUIRE(destroyed);
  }

  SECTION("from a worker thread to the event loop")
  {
    QThread worker;
    QObject worker_context;
    worker_context.moveToThread(&worker);
    worker.start();

    bool on_worker = false;
    bool back_on_main = false;

    co_spawn(qt::executor {}, [&]() -> async<void> {
      co_await schedule_on(qt::executor { &worker_context });
      on_worker = QThread::currentThread() == &worker;

      co_await schedule_on(qt::executor {});
      back_on_main = QThread::currentThread() == app.thread();

      app.quit();
    });

    app.exec();
    worker.quit();
    worker.wait();

    REQUIRE(on_worker);
    REQUIRE(back_on_main);
  }
}
