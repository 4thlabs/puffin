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

#ifndef PUFFIN_REST_ERRORS_HPP
#define PUFFIN_REST_ERRORS_HPP

#include <puffin/async/try_await.hpp>
#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/status.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace puffin {
namespace rest {

/**
 * @brief An HTTP error status.
 *
 * Server side, thrown by a service or an interceptor to answer this status, the message being sent as
 * {"error": message}. Client side, thrown by client::call when the response is not 2xx, the message being
 * the response body.
 */
class http_error : public std::runtime_error {
public:
  explicit http_error(webkit::status status, std::string message = {})
      : http_error(static_cast<int>(status), std::move(message))
  {}

  explicit http_error(int status, std::string message = {})
      : std::runtime_error(message.empty() ? std::string(webkit::reason_phrase(status)) : message),
        status_(status), body_(std::move(message))
  {}

  http_error(int status, std::string body, webkit::headers headers)
      : http_error(status, std::move(body))
  {
    headers_ = std::move(headers);
  }

  int status() const noexcept { return status_; }

  /// The message (server side) or the response body (client side)
  const std::string& body() const noexcept { return body_; }

  /// Response headers, client side
  const webkit::headers& headers() const noexcept { return headers_; }

private:
  int status_;
  std::string body_;
  webkit::headers headers_;
};

/**
 * @brief The value of a call, or the HTTP error it failed with (client::try_call)
 */
template<typename T>
class result {
public:
  using value_type = T;

  result(T value)
      : state_(std::in_place_index<0>, std::move(value))
  {}

  result(http_error error)
      : state_(std::in_place_index<1>, std::move(error))
  {}

  bool has_value() const noexcept { return state_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  T& value() &
  {
    if (!has_value())
      throw std::get<1>(state_);

    return std::get<0>(state_);
  }

  T&& value() && { return std::move(value()); }

  T& operator*() & { return value(); }
  T* operator->() { return &value(); }

  const http_error& error() const { return std::get<1>(state_); }

private:
  std::variant<T, http_error> state_;
};

template<>
class result<void> {
public:
  using value_type = void;

  result() = default;

  result(http_error error)
      : error_(std::in_place, std::move(error))
  {}

  bool has_value() const noexcept { return !error_.has_value(); }
  explicit operator bool() const noexcept { return has_value(); }

  void value() const
  {
    if (error_)
      throw *error_;
  }

  const http_error& error() const { return *error_; }

private:
  std::optional<http_error> error_;
};

namespace detail {

/// The http_error of a failed outcome; any other exception is rethrown
template<typename V>
http_error http_error_of(const async::outcome<V>& outcome)
{
  if (outcome)
    throw std::logic_error("http_error_of: the outcome holds a value");

  try {
    outcome.rethrow();
  } catch (const http_error& e) {
    return e;
  }
}

} // namespace detail

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_ERRORS_HPP
