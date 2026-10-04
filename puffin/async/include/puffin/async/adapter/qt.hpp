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

#ifndef PUFFIN_ASYNC_ADAPTER_QT_HPP
#define PUFFIN_ASYNC_ADAPTER_QT_HPP

// Qt 6 adapter for puffin::async

#include <puffin/async/from_callback.hpp>

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <chrono>
#include <coroutine>
#include <tuple>
#include <type_traits>
#include <utility>

namespace puffin {
namespace async {
namespace qt {

/**
 * @brief Executor resuming coroutines from the event loop of the thread a QObject lives in.
 *        Lightweight handle, pass it by value: co_spawn(qt::executor {}, task());
 *
 * Coroutines posted after the context object is destroyed are never resumed.
 */
class executor {
public:
  /**
   * @brief Uses the application object (main thread) when no context is given
   */
  explicit executor(QObject* context = QCoreApplication::instance())
      : context_(context)
  {}

  void post(std::coroutine_handle<> h) const
  {
    if (context_)
      QMetaObject::invokeMethod(context_.data(), [h] { h.resume(); }, Qt::QueuedConnection);
  }

  /// Resumes h once delay elapsed, with a single shot QTimer
  void post_after(std::coroutine_handle<> h, std::chrono::nanoseconds delay) const
  {
    if (context_)
      QTimer::singleShot(std::chrono::ceil<std::chrono::milliseconds>(delay), context_.data(), [h] { h.resume(); });
  }

  QObject* context() const noexcept { return context_.data(); }

  friend bool operator==(const executor& lhs, const executor& rhs) noexcept
  {
    return lhs.context_.data() == rhs.context_.data();
  }

private:
  QPointer<QObject> context_;
};

namespace detail {

// Same trick as Qt's own QtFuture::connect: QPrivateSignal finds itself as injected class name
template<typename T, typename = void>
inline constexpr bool is_private_signal_arg = false;

template<typename T>
inline constexpr bool is_private_signal_arg<T, std::enable_if_t<std::is_class_v<class T::QPrivateSignal>>> = true;

template<typename... Args>
struct type_list {};

template<typename List, typename... Args>
struct filter_private_signal;

template<typename... Kept>
struct filter_private_signal<type_list<Kept...>> {
  using type = type_list<Kept...>;
};

template<typename... Kept, typename Last>
struct filter_private_signal<type_list<Kept...>, Last> {
  using type = std::conditional_t<is_private_signal_arg<std::decay_t<Last>>, type_list<Kept...>, type_list<Kept..., Last>>;
};

template<typename... Kept, typename First, typename Second, typename... Rest>
struct filter_private_signal<type_list<Kept...>, First, Second, Rest...>
    : filter_private_signal<type_list<Kept..., First>, Second, Rest...> {};

template<typename Signal>
struct signal_traits;

template<typename Class, typename... Args>
struct signal_traits<void (Class::*)(Args...)> {
  using class_type = Class;
  using arguments = typename filter_private_signal<type_list<>, Args...>::type;
};

template<typename Sender, typename Signal, typename... Args>
auto make_signal_awaitable(Sender* sender, Signal signal, type_list<Args...>)
{
  return from_callback<std::decay_t<Args>...>([sender, signal](auto callback) {
    QObject::connect(
        sender, signal, sender, [callback](const std::decay_t<Args>&... args) { callback(args...); },
        Qt::SingleShotConnection);
  });
}

}

/**
 * @brief Awaits the next emission of a signal and returns its arguments (nothing, the value, or a
 *        tuple), private signal tag excluded:
 *
 *   QObject* object = co_await qt::signal(object, &QObject::destroyed);
 *
 * The coroutine is resumed on its own executor. It is never resumed if the sender is destroyed
 * without emitting the signal.
 */
template<typename Sender, typename Signal>
auto signal(Sender* sender, Signal signal)
{
  using traits = detail::signal_traits<Signal>;
  static_assert(std::is_base_of_v<typename traits::class_type, Sender>, "the signal must belong to the sender");

  return detail::make_signal_awaitable(sender, signal, typename traits::arguments {});
}

/**
 * @brief Suspends the coroutine for the given duration, using a single shot timer in the thread of
 *        the context object (the application object by default)
 */
template<typename Rep, typename Period>
auto sleep_for(std::chrono::duration<Rep, Period> duration, QObject* context = QCoreApplication::instance())
{
  auto ms = std::chrono::ceil<std::chrono::milliseconds>(duration);

  return from_callback<>([ms, context](auto callback) { QTimer::singleShot(ms, context, callback); });
}

}
}
}

#endif // PUFFIN_ASYNC_ADAPTER_QT_HPP
