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

#ifndef PUFFIN_REST_PARAMS_HPP
#define PUFFIN_REST_PARAMS_HPP

#include <puffin/rest/fixed_string.hpp>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace puffin {
namespace rest {

/**
 * @brief Conversion of path, query and header parameters from and to text.
 *
 * Specialize it for your own types (uuid, dates...):
 *
 *   template<>
 *   struct puffin::rest::param_traits<my_id> {
 *     static std::optional<my_id> parse(std::string_view text);
 *     static std::string format(const my_id& value);
 *   };
 */
template<typename T, typename Enable = void>
struct param_traits;

template<typename T>
concept Param = requires(std::string_view text, const T& value) {
  { param_traits<T>::parse(text) } -> std::same_as<std::optional<T>>;
  { param_traits<T>::format(value) } -> std::convertible_to<std::string>;
};

template<typename T>
struct param_traits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
  static std::optional<T> parse(std::string_view text)
  {
    T value {};
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (ec != std::errc() || end != text.data() + text.size() || text.empty())
      return std::nullopt;

    return value;
  }

  static std::string format(T value) { return std::to_string(value); }
};

template<typename T>
struct param_traits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
  static std::optional<T> parse(std::string_view text)
  {
    T value {};
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (ec != std::errc() || end != text.data() + text.size() || text.empty())
      return std::nullopt;

    return value;
  }

  static std::string format(T value)
  {
    char buffer[64];
    auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, end);
  }
};

template<>
struct param_traits<bool> {
  static std::optional<bool> parse(std::string_view text)
  {
    if (text == "true" || text == "1")
      return true;

    if (text == "false" || text == "0")
      return false;

    return std::nullopt;
  }

  static std::string format(bool value) { return value ? "true" : "false"; }
};

template<>
struct param_traits<std::string> {
  static std::optional<std::string> parse(std::string_view text) { return std::string(text); }
  static std::string format(const std::string& value) { return value; }
};

/**
 * @brief param_traits for an enum whose values are named in order:
 *
 *   template<>
 *   struct puffin::rest::param_traits<card_status> : enum_param<card_status, "active", "frozen"> {};
 */
template<typename E, fixed_string... Names>
struct enum_param {
  static_assert(std::is_enum_v<E>, "enum_param is for enums");

  static std::optional<E> parse(std::string_view text)
  {
    std::size_t index = 0;
    std::optional<E> result;
    ((text == Names.view() ? (void)(result = static_cast<E>(index)) : (void)0, ++index), ...);
    return result;
  }

  static std::string format(E value)
  {
    static constexpr std::string_view names[] = {Names.view()...};
    auto index = static_cast<std::size_t>(value);
    return index < sizeof...(Names) ? std::string(names[index]) : std::to_string(index);
  }
};

/// A bearer token, read from and written to "Authorization: Bearer <token>"
struct bearer {
  std::string token;

  bool operator==(const bearer&) const = default;
};

template<>
struct param_traits<bearer> {
  static std::optional<bearer> parse(std::string_view text)
  {
    constexpr std::string_view scheme = "Bearer ";

    if (text.size() <= scheme.size())
      return std::nullopt;

    for (std::size_t i = 0; i < scheme.size(); ++i) {
      if ((text[i] | 0x20) != (scheme[i] | 0x20))
        return std::nullopt;
    }

    return bearer {std::string(text.substr(scheme.size()))};
  }

  static std::string format(const bearer& value) { return "Bearer " + value.token; }
};

namespace detail {

template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

/// The type parsed by param_traits: T, or the element of optional<T> and vector<T>
template<typename T>
struct param_element {
  using type = T;
};

template<typename T>
struct param_element<std::optional<T>> {
  using type = T;
};

template<typename T, typename A>
struct param_element<std::vector<T, A>> {
  using type = T;
};

template<typename T>
using param_element_t = typename param_element<T>::type;

} // namespace detail

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_PARAMS_HPP
