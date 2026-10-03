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

#ifndef PUFFIN_ASYNC_SYNC_WAIT_HPP
#define PUFFIN_ASYNC_SYNC_WAIT_HPP

#include <puffin/async/async.hpp>
#include <puffin/async/co_spawn.hpp>
#include <puffin/async/executor.hpp>

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <type_traits>

namespace puffin {
namespace async {

/**
 * @brief Blocks the calling thread until the task is done and returns its result (or rethrows).
 *
 * With an executor, the task is started on it. Without, it starts inline on the calling thread
 * and continues wherever its awaits resume it. Never call it from a thread the task needs to make
 * progress (e.g. the thread running its executor).
 */
template<typename T>
T sync_wait(any_executor executor, async<T> task)
{
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;

  std::exception_ptr exception;
  std::conditional_t<std::is_void_v<T>, bool, std::optional<T>> value {};

  auto entry = [&]() -> detail::detached_task {
    try {
      if constexpr (std::is_void_v<T>)
        co_await std::move(task);
      else
        value.emplace(co_await std::move(task));
    } catch (...) {
      exception = std::current_exception();
    }

    std::lock_guard<std::mutex> lock(mutex);
    done = true;
    cv.notify_all();
  };

  entry().start(std::move(executor));

  std::unique_lock<std::mutex> lock(mutex);
  cv.wait(lock, [&] { return done; });

  if (exception)
    std::rethrow_exception(exception);

  if constexpr (!std::is_void_v<T>)
    return std::move(*value);
}

template<typename T>
T sync_wait(async<T> task)
{
  return sync_wait(any_executor {}, std::move(task));
}

}
}

#endif // PUFFIN_ASYNC_SYNC_WAIT_HPP
