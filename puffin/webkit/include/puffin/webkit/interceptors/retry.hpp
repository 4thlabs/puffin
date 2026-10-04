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

#ifndef PUFFIN_WEBKIT_INTERCEPTORS_RETRY_HPP
#define PUFFIN_WEBKIT_INTERCEPTORS_RETRY_HPP

#include <puffin/async/async.hpp>
#include <puffin/async/try_await.hpp>
#include <puffin/webkit/client/interceptor.hpp>
#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/http/errors.hpp>
#include <puffin/webkit/http/method.hpp>
#include <puffin/webkit/http/status.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace puffin {
namespace webkit {
namespace detail {

/// Statuses worth sending an idempotent request again for
constexpr bool retryable_status(int code) noexcept
{
  return code == static_cast<int>(status::bad_gateway) || code == static_cast<int>(status::service_unavailable) ||
         code == static_cast<int>(status::gateway_timeout);
}

/// False for the failures sending again cannot fix: a malformed response, a misuse of the client
inline bool transient_failure(const std::exception_ptr& error)
{
  try {
    std::rethrow_exception(error);
  } catch (const protocol_error&) {
    return false;
  } catch (const std::logic_error&) {
    return false;
  } catch (...) {
    return true;
  }
}

/// True if an attempt ended in a way worth trying again
template<typename Outcome>
bool worth_retrying(const Outcome& outcome)
{
  return outcome ? retryable_status(outcome.value->status_code()) : transient_failure(outcome.error);
}

/// The delay a response asks for with "Retry-After: <seconds>" (an HTTP date is not supported), at most a day
inline std::optional<std::chrono::milliseconds> retry_after(const response& res)
{
  auto value = res.headers().get("Retry-After");
  auto seconds = value ? parse_decimal(trim(*value)) : std::nullopt;

  if (!seconds)
    return std::nullopt;

  return std::chrono::seconds(std::min<std::size_t>(*seconds, 24 * 3600));
}

/// The exponential backoff before the attempt after `attempt`: first, doubled each time, up to max
constexpr std::chrono::milliseconds backoff_delay(std::size_t attempt, std::chrono::milliseconds first,
                                                  std::chrono::milliseconds max) noexcept
{
  auto delay = first;

  for (std::size_t i = 1; i < attempt && delay < max; ++i)
    delay *= 2;

  return std::min(delay, max);
}

static_assert(backoff_delay(1, std::chrono::milliseconds(100), std::chrono::seconds(5)).count() == 100);
static_assert(backoff_delay(3, std::chrono::milliseconds(100), std::chrono::seconds(5)).count() == 400);
static_assert(backoff_delay(20, std::chrono::milliseconds(100), std::chrono::seconds(5)).count() == 5000);

} // namespace detail

namespace interceptors {

/**
 * @brief Sends idempotent requests again when the transport fails or the server answers 502, 503 or 504.
 *
 * Attempts are spaced by an exponential backoff (100 ms, then doubled, up to 5 s), or by the Retry-After of the
 * response (seconds, capped at the maximum delay). Waiting depends on the event loop, so it is given:
 *
 *   client.interceptor<interceptors::retry>().sleep([ex](auto delay) { return async::asio::sleep_for(ex, delay); });
 *
 * Without a sleep function, attempts follow each other immediately. A malformed response is not retried.
 */
class retry {
public:
  using sleep_function = std::function<async::async<void>(std::chrono::milliseconds)>;

  /// Total number of attempts, 3 by default
  void attempts(std::size_t n) { attempts_ = n == 0 ? 1 : n; }
  std::size_t attempts() const noexcept { return attempts_; }

  /// Delay before the second attempt, doubled before each next one up to max_delay
  void backoff(std::chrono::milliseconds first_delay, std::chrono::milliseconds max_delay)
  {
    first_delay_ = first_delay;
    max_delay_ = std::max(first_delay, max_delay);
  }

  void sleep(sleep_function sleep) { sleep_ = std::move(sleep); }

  async::async<response> operator()(request& req, next_request next) const
  {
    if (!idempotent(req.method()))
      co_return co_await next(req);

    for (std::size_t attempt = 1;; ++attempt) {
      auto outcome = co_await async::try_await(next(req));

      if (attempt >= attempts_ || !detail::worth_retrying(outcome)) {
        if (!outcome)
          outcome.rethrow();

        co_return std::move(*outcome.value);
      }

      if (sleep_)
        co_await sleep_(delay(attempt, outcome ? &*outcome.value : nullptr));
    }
  }

private:
  /// The delay after a failed attempt: the Retry-After of its response, or the backoff
  std::chrono::milliseconds delay(std::size_t attempt, const response* res) const
  {
    auto asked = res ? detail::retry_after(*res) : std::nullopt;
    return std::min(asked.value_or(detail::backoff_delay(attempt, first_delay_, max_delay_)), max_delay_);
  }

  std::size_t attempts_ = 3;
  std::chrono::milliseconds first_delay_ {100};
  std::chrono::milliseconds max_delay_ {5000};
  sleep_function sleep_;
};

} // namespace interceptors
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_INTERCEPTORS_RETRY_HPP
