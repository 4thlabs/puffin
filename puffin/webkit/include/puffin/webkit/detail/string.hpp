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

#ifndef PUFFIN_WEBKIT_DETAIL_STRING_HPP
#define PUFFIN_WEBKIT_DETAIL_STRING_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {
namespace detail {

inline constexpr char to_lower(char c) noexcept
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/**
 * @brief Case insensitive comparison, as required for header names
 */
inline constexpr bool iequals(std::string_view lhs, std::string_view rhs) noexcept
{
  if (lhs.size() != rhs.size())
    return false;

  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (to_lower(lhs[i]) != to_lower(rhs[i]))
      return false;
  }

  return true;
}

/**
 * @brief Removes leading and trailing spaces and tabs
 */
inline constexpr std::string_view trim(std::string_view s) noexcept
{
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.remove_prefix(1);

  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.remove_suffix(1);

  return s;
}

inline constexpr bool is_token_char(char c) noexcept
{
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
    return true;

  switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
    case '-': case '.': case '^': case '_': case '`': case '|': case '~':
      return true;
    default:
      return false;
  }
}

/**
 * @brief RFC 9110 token, used for methods and header names
 */
inline constexpr bool is_token(std::string_view s) noexcept
{
  if (s.empty())
    return false;

  for (char c : s) {
    if (!is_token_char(c))
      return false;
  }

  return true;
}

/**
 * @brief True if s can be sent as a header value or reason phrase: no control character but HTAB
 *        (so no CR, LF or NUL, which would allow response splitting)
 */
inline constexpr bool is_field_value(std::string_view s) noexcept
{
  for (char c : s) {
    auto u = static_cast<unsigned char>(c);

    if ((u < 0x20 && c != '\t') || u == 0x7F)
      return false;
  }

  return true;
}

/**
 * @brief True if s can be sent as a request target: not empty, no space and no control character
 */
inline constexpr bool is_target(std::string_view s) noexcept
{
  if (s.empty())
    return false;

  for (char c : s) {
    auto u = static_cast<unsigned char>(c);

    if (u <= 0x20 || u == 0x7F)
      return false;
  }

  return true;
}

/**
 * @brief RFC 6265 cookie-octet: printable US-ASCII but space, '"', ',', ';' and backslash
 */
inline constexpr bool is_cookie_value(std::string_view s) noexcept
{
  for (char c : s) {
    auto u = static_cast<unsigned char>(c);

    if (u <= 0x20 || u >= 0x7F || c == '"' || c == ',' || c == ';' || c == '\\')
      return false;
  }

  return true;
}

inline constexpr int hex_value(char c) noexcept
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/**
 * @brief Parses an hexadecimal number (chunk sizes), nullopt on invalid input or overflow
 */
inline std::optional<std::size_t> parse_hex(std::string_view s) noexcept
{
  if (s.empty())
    return std::nullopt;

  std::size_t value = 0;

  for (char c : s) {
    int v = hex_value(c);

    if (v < 0 || value > (static_cast<std::size_t>(-1) >> 4))
      return std::nullopt;

    value = (value << 4) | static_cast<std::size_t>(v);
  }

  return value;
}

/**
 * @brief Parses a decimal number (Content-Length), nullopt on invalid input or overflow
 */
inline std::optional<std::size_t> parse_decimal(std::string_view s) noexcept
{
  if (s.empty())
    return std::nullopt;

  std::size_t value = 0;

  for (char c : s) {
    if (c < '0' || c > '9')
      return std::nullopt;

    std::size_t digit = static_cast<std::size_t>(c - '0');

    if (value > (static_cast<std::size_t>(-1) - digit) / 10)
      return std::nullopt;

    value = value * 10 + digit;
  }

  return value;
}

/**
 * @brief Percent-encodes everything but RFC 3986 unreserved characters
 */
inline std::string percent_encode(std::string_view s)
{
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size());

  for (unsigned char c : s) {
    bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                      c == '_' || c == '.' || c == '~';

    if (unreserved) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += digits[c >> 4];
      out += digits[c & 0x0F];
    }
  }

  return out;
}

/**
 * @brief Decodes %XX sequences, invalid sequences are kept as is
 */
inline std::string percent_decode(std::string_view s)
{
  std::string out;
  out.reserve(s.size());

  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      int hi = hex_value(s[i + 1]);
      int lo = hex_value(s[i + 2]);

      if (hi >= 0 && lo >= 0) {
        out += static_cast<char>((hi << 4) | lo);
        i += 2;
        continue;
      }
    }

    out += s[i];
  }

  return out;
}

/**
 * @brief Joins values with a separator: {"GET", "POST"} -> "GET, POST"
 */
inline std::string join(const std::vector<std::string>& values, std::string_view separator)
{
  std::string out;

  for (const auto& value : values) {
    if (!out.empty())
      out += separator;

    out += value;
  }

  return out;
}

} // namespace detail
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_DETAIL_STRING_HPP
