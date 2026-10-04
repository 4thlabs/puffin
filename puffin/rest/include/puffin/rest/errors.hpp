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
#include <puffin/rest/codec.hpp>
#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/status.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace puffin {
namespace rest {

/**
 * @brief An HTTP error status.
 *
 * Server side, thrown by a service to answer this status, the message being sent to the client by the error
 * codec of the mount ({"error": message} by default). Client side, thrown by client::call when the response is not 2xx, the message being
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

/// Length of a UTF-8 sequence from its first byte, 0 for a byte that cannot start one
constexpr std::size_t utf8_length(unsigned char lead) noexcept
{
  if (lead < 0x80)
    return 1;

  if (lead >= 0xC2 && lead < 0xE0)
    return 2;

  if (lead >= 0xE0 && lead < 0xF0)
    return 3;

  return lead >= 0xF0 && lead < 0xF5 ? 4 : 0;
}

/// Length of the valid UTF-8 sequence starting at s[i], 0 if the bytes there are not valid UTF-8
inline std::size_t utf8_sequence_length(std::string_view s, std::size_t i)
{
  auto byte = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
  const unsigned char lead = byte(i);
  const std::size_t length = utf8_length(lead);

  if (length == 0 || i + length > s.size())
    return 0;

  for (std::size_t k = 1; k < length; ++k) {
    if ((byte(i + k) & 0xC0) != 0x80)
      return 0;
  }

  // Overlong 3 and 4 byte forms, UTF-16 surrogates, code points past U+10FFFF
  const unsigned char second = length > 1 ? byte(i + 1) : 0;
  const bool invalid = (lead == 0xE0 && second < 0xA0) || (lead == 0xED && second >= 0xA0) ||
                       (lead == 0xF0 && second < 0x90) || (lead == 0xF4 && second >= 0x90);
  return invalid ? 0 : length;
}

/// A string as the inside of a JSON string literal; bytes that are not valid UTF-8 become U+FFFD
inline std::string json_escape(std::string_view s)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;

  for (std::size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    const std::size_t length = utf8_sequence_length(s, i);

    if (length == 0) {
      out += "\\ufffd";
      ++i;
      continue;
    }

    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      out += "\\u00";
      out += digits[c >> 4];
      out += digits[c & 0x0F];
    } else {
      out.append(s.substr(i, length));
    }

    i += length;
  }

  return out;
}

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

  throw std::logic_error("http_error_of: rethrow returned"); // Unreachable, rethrow always throws
}

} // namespace detail

/// How a mount writes the message of an http_error (rest::errors)
template<typename C>
concept ErrorCodec = requires(const C& codec, const http_error& e) {
  { C::content_type } -> std::convertible_to<std::string_view>;
  { codec.encode(e) } -> std::convertible_to<std::string>;
};

struct json_error_codec {
  static constexpr std::string_view content_type = "application/json";

  /// {"error": message}
  std::string encode(const http_error& e) const { return "{\"error\":\"" + detail::json_escape(e.what()) + "\"}"; }
};

/// The message as plain text
struct text_error_codec {
  static constexpr std::string_view content_type = text_codec::content_type;

  std::string encode(const http_error& e) const { return e.what(); }
};

/// The error codec of a mount, json_error_codec when not given
template<ErrorCodec Codec>
struct error_codec_option {
  Codec value;
};

template<typename Codec>
  requires ErrorCodec<std::decay_t<Codec>>
error_codec_option<std::decay_t<Codec>> errors(Codec&& codec)
{
  return {std::forward<Codec>(codec)};
}

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_ERRORS_HPP
