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
#include <puffin/webkit/http/method.hpp>
#include <puffin/webkit/http/status.hpp>

#include <cstddef>
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

} // namespace detail

namespace interceptors {

/**
 * @brief Sends idempotent requests again when the transport fails or the server answers 502, 503 or 504.
 */
class retry {
public:
  /// Total number of attempts, 3 by default
  void attempts(std::size_t n) { attempts_ = n == 0 ? 1 : n; }
  std::size_t attempts() const noexcept { return attempts_; }

  async::async<response> operator()(request& req, next_request next) const
  {
    if (!idempotent(req.method()))
      co_return co_await next(req);

    for (std::size_t attempt = 1;; ++attempt) {
      auto outcome = co_await async::try_await(next(req));

      if (attempt >= attempts_ || (outcome && !detail::retryable_status(outcome.value->status_code()))) {
        if (!outcome)
          outcome.rethrow();

        co_return std::move(*outcome.value);
      }
    }
  }

private:
  std::size_t attempts_ = 3;
};

} // namespace interceptors
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_INTERCEPTORS_RETRY_HPP
