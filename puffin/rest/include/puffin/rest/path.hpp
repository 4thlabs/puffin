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

#ifndef PUFFIN_REST_PATH_HPP
#define PUFFIN_REST_PATH_HPP

#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/fixed_string.hpp>
#include <puffin/rest/params.hpp>
#include <puffin/webkit/detail/string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace puffin {
namespace rest {

/**
 * @brief Types of the path parameters: "/users/{id:int}/files/{name}"
 *
 * string (the default) matches one segment, path matches the rest of the path, slashes included.
 */
enum class param_kind { string, int32, int64, uint32, uint64, float64, boolean, path };

namespace detail {

// Not constexpr: calling it while parsing a path template at compile time stops the compilation with this message
inline void path_template_error(const char*) {}

struct path_param_info {
  std::size_t name_begin = 0;
  std::size_t name_size = 0;
  param_kind kind = param_kind::string;
};

constexpr std::size_t count_path_params(std::string_view path)
{
  std::size_t count = 0;

  for (char c : path)
    count += c == '{';

  return count;
}

constexpr param_kind path_param_kind(std::string_view type)
{
  if (type.empty() || type == "string")
    return param_kind::string;
  if (type == "int")
    return param_kind::int32;
  if (type == "int64")
    return param_kind::int64;
  if (type == "uint")
    return param_kind::uint32;
  if (type == "uint64")
    return param_kind::uint64;
  if (type == "double")
    return param_kind::float64;
  if (type == "bool")
    return param_kind::boolean;
  if (type == "path")
    return param_kind::path;

  path_template_error("unknown path parameter type, expected int, int64, uint, uint64, double, bool, string or path");
  return param_kind::string;
}

template<std::size_t Count>
constexpr std::array<path_param_info, Count> parse_path_params(std::string_view path)
{
  std::array<path_param_info, Count> params {};
  std::size_t index = 0;

  for (std::size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '}')
      path_template_error("unexpected '}' in path template");

    if (path[i] != '{')
      continue;

    std::size_t close = path.find('}', i);

    if (close == std::string_view::npos)
      path_template_error("unclosed '{' in path template");

    std::string_view param = path.substr(i + 1, close - i - 1);

    if (param.find('{') != std::string_view::npos)
      path_template_error("nested '{' in path template");

    std::size_t colon = param.find(':');
    std::string_view name = param.substr(0, colon);

    if (name.empty())
      path_template_error("path parameter without name");

    for (std::size_t p = 0; p < index; ++p) {
      if (path.substr(params[p].name_begin, params[p].name_size) == name)
        path_template_error("duplicate path parameter name");
    }

    params[index].name_begin = i + 1;
    params[index].name_size = name.size();
    params[index].kind = path_param_kind(colon == std::string_view::npos ? std::string_view() : param.substr(colon + 1));

    if (params[index].kind == param_kind::path && close + 1 != path.size())
      path_template_error("a path parameter must be the last element of the path");

    ++index;
    i = close;
  }

  return params;
}

template<param_kind K>
struct kind_type;

template<>
struct kind_type<param_kind::string> {
  using type = std::string;
};

template<>
struct kind_type<param_kind::int32> {
  using type = int;
};

template<>
struct kind_type<param_kind::int64> {
  using type = std::int64_t;
};

template<>
struct kind_type<param_kind::uint32> {
  using type = unsigned;
};

template<>
struct kind_type<param_kind::uint64> {
  using type = std::uint64_t;
};

template<>
struct kind_type<param_kind::float64> {
  using type = double;
};

template<>
struct kind_type<param_kind::boolean> {
  using type = bool;
};

template<>
struct kind_type<param_kind::path> {
  using type = std::string;
};

inline std::string_view kind_regex(param_kind kind)
{
  switch (kind) {
    case param_kind::int32:
    case param_kind::int64: return "-?[0-9]+";
    case param_kind::uint32:
    case param_kind::uint64: return "[0-9]+";
    case param_kind::float64: return "[-+0-9.eE]+";
    case param_kind::boolean: return "true|false|1|0";
    case param_kind::path: return ".+";
    default: return "[^/]+";
  }
}

} // namespace detail

/**
 * @brief A path template parsed at compile time: parameter names and types, the regular expression
 *        used by the router, and the formatting of concrete paths for the client.
 */
template<fixed_string Path>
struct path_template {
  static constexpr std::string_view text = Path.view();
  static constexpr std::size_t param_count = detail::count_path_params(Path.view());
  static constexpr auto params = detail::parse_path_params<param_count>(Path.view());

  static constexpr std::string_view param_name(std::size_t i)
  {
    return text.substr(params[i].name_begin, params[i].name_size);
  }

  static constexpr param_kind kind(std::size_t i) { return params[i].kind; }

  template<std::size_t I>
  using param_type = typename detail::kind_type<params[I].kind>::type;

private:
  template<std::size_t... Is>
  static auto make_types(std::index_sequence<Is...>) -> detail::typelist<param_type<Is>...>;

public:
  /// typelist of the parameter types, in order
  using types = decltype(make_types(std::make_index_sequence<param_count>()));

  /// The regular expression matching the path, one capture group per parameter
  static std::string regex()
  {
    std::string re;
    std::size_t param = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
      char c = text[i];

      if (c == '{') {
        re += '(';
        re += detail::kind_regex(params[param++].kind);
        re += ')';
        i = text.find('}', i);
        continue;
      }

      if (std::string_view(".^$|()[]*+?\\").find(c) != std::string_view::npos)
        re += '\\';

      re += c;
    }

    return re;
  }

  /// Builds a concrete path from the parameter values, percent-encoded
  template<typename... Values>
  static std::string format(const Values&... values)
  {
    static_assert(sizeof...(Values) == param_count, "wrong number of path parameters");
    return format_impl(std::index_sequence_for<Values...>(), values...);
  }

  /// Converts the raw value of a captured parameter, nullopt if it is not valid
  template<std::size_t I>
  static std::optional<param_type<I>> parse(std::string_view raw)
  {
    return param_traits<param_type<I>>::parse(webkit::detail::percent_decode(raw));
  }

private:
  template<std::size_t... Is, typename... Values>
  static std::string format_impl(std::index_sequence<Is...>, const Values&... values)
  {
    // Leading empty string so that the array is never empty
    std::string formatted[] = {std::string(), encode(param_traits<Values>::format(values), params[Is].kind)...};
    std::string path;
    std::size_t param = 1;

    for (std::size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '{') {
        path += formatted[param++];
        i = text.find('}', i);
        continue;
      }

      path += text[i];
    }

    return path;
  }

  static std::string encode(const std::string& value, param_kind kind)
  {
    if (kind != param_kind::path)
      return webkit::detail::percent_encode(value);

    std::string out;
    std::size_t begin = 0;

    for (;;) {
      std::size_t slash = value.find('/', begin);
      out += webkit::detail::percent_encode(std::string_view(value).substr(begin, slash - begin));

      if (slash == std::string::npos)
        break;

      out += '/';
      begin = slash + 1;
    }

    return out;
  }
};

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_PATH_HPP
