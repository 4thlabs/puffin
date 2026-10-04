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

#ifndef PUFFIN_REST_DETAIL_ARGUMENTS_HPP
#define PUFFIN_REST_DETAIL_ARGUMENTS_HPP

#include <puffin/rest/codec.hpp>
#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/rest/params.hpp>
#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/http/headers.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/uri/uri.hpp>

#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

// Conversion of endpoint arguments to and from HTTP requests: decoded by the server, encoded by the client.

namespace puffin {
namespace rest {
namespace detail {

template<typename D>
struct is_path_arg : std::false_type {};

template<fixed_string Path, std::size_t I>
struct is_path_arg<path_arg<Path, I>> : std::true_type {};

template<typename D>
struct is_query : std::false_type {};

template<fixed_string Name, typename T>
struct is_query<query<Name, T>> : std::true_type {};

template<typename D>
struct is_header : std::false_type {};

template<fixed_string Name, typename T>
struct is_header<header<Name, T>> : std::true_type {};

//
// Text parameters (path, query, header)
//

template<typename D>
constexpr std::string_view arg_kind() noexcept
{
  if constexpr (is_path_arg<D>::value)
    return "path parameter";
  else if constexpr (is_query<D>::value)
    return "query parameter";
  else
    return "header";
}

[[noreturn]] inline void bad_argument(std::string_view problem, std::string_view kind, std::string_view name)
{
  throw http_error(webkit::status::bad_request,
                   std::string(problem) + " " + std::string(kind) + " '" + std::string(name) + "'");
}

/// Parses one value of argument D, 400 if it is not valid
template<typename D, typename T>
T parse_text(std::string_view raw, std::string_view name)
{
  auto value = param_traits<T>::parse(raw);

  if (!value)
    bad_argument("invalid", arg_kind<D>(), name);

  return std::move(*value);
}

/// Parses a single text argument of type T or optional<T>, 400 if it is required and absent
template<typename D, typename T>
T parse_single(std::optional<std::string_view> raw, std::string_view name)
{
  if constexpr (is_optional<T>::value) {
    if (!raw)
      return std::nullopt;

    return parse_text<D, param_element_t<T>>(*raw, name);
  } else {
    if (!raw)
      bad_argument("missing", arg_kind<D>(), name);

    return parse_text<D, T>(*raw, name);
  }
}

/// Calls emit with the text of each value: none for an empty optional, one per element of a vector
template<typename V, typename F>
void format_text(const V& value, F&& emit)
{
  using element = param_element_t<V>;

  if constexpr (is_vector<V>::value) {
    for (const auto& v : value)
      emit(param_traits<element>::format(v));
  } else if constexpr (is_optional<V>::value) {
    if (value)
      emit(param_traits<element>::format(*value));
  } else {
    emit(param_traits<element>::format(value));
  }
}

inline void append_query(std::string& target, std::string_view name, std::string_view value)
{
  target += target.find('?') == std::string::npos ? '?' : '&';
  target += webkit::detail::percent_encode(name);
  target += '=';
  target += webkit::detail::percent_encode(value);
}

//
// Server side: from the request to the argument values
//

/// What arguments are decoded from: the context of the request and its parsed query
template<typename Ctx>
struct incoming {
  Ctx& ctx;
  const webkit::query_map& query;
};

template<typename D, typename Ctx>
typename D::value_type decode_path(incoming<Ctx>& in)
{
  auto value = D::template_type::template parse<D::index>(in.ctx.param(D::index));

  if (!value)
    bad_argument("invalid", arg_kind<D>(), D::name);

  return std::move(*value);
}

template<typename D, typename Ctx>
typename D::value_type decode_body(incoming<Ctx>& in)
{
  using T = typename D::value_type;
  using codec = resolve_codec_t<T, typename D::codec>;
  const webkit::request& req = in.ctx.request();
  auto content_type = req.headers().get("Content-Type");

  if (content_type && !same_media_type(*content_type, codec::content_type))
    throw http_error(webkit::status::unsupported_media_type, "expected " + std::string(codec::content_type));

  try {
    return codec::template decode<T>(req.body());
  } catch (const std::exception& e) {
    throw http_error(webkit::status::bad_request, std::string("invalid body: ") + e.what());
  }
}

template<typename D, typename Ctx>
typename D::value_type decode_query(incoming<Ctx>& in)
{
  using T = typename D::value_type;
  std::string_view name = D::name.view();

  if constexpr (is_vector<T>::value) {
    T values;

    for (auto [it, end] = in.query.equal_range(name); it != end; ++it)
      values.push_back(parse_text<D, param_element_t<T>>(it->second, name));

    return values;
  } else {
    auto it = in.query.find(name);
    return parse_single<D, T>(it == in.query.end() ? std::nullopt : std::optional<std::string_view>(it->second), name);
  }
}

template<typename D, typename Ctx>
typename D::value_type decode_arg(incoming<Ctx>& in)
{
  if constexpr (is_path_arg<D>::value)
    return decode_path<D>(in);
  else if constexpr (is_body<D>::value)
    return decode_body<D>(in);
  else if constexpr (is_query<D>::value)
    return decode_query<D>(in);
  else
    return parse_single<D, typename D::value_type>(in.ctx.request().headers().get(D::name.view()), D::name.view());
}

/// The arguments of endpoint R, as a tuple, decoded in order. Invalid arguments throw http_error (400, 415).
template<typename R, typename Ctx>
auto decode_args(Ctx& ctx)
{
  // Parsed only when an argument reads it
  const webkit::query_map query = filter_t<is_query, typename R::args>::size > 0 ? ctx.request().query()
                                                                                : webkit::query_map {};
  incoming<Ctx> in {ctx, query};

  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    // Braced initialization: decoded in order
    return apply_t<std::tuple, typename R::values> {decode_arg<at_t<Is, typename R::args>>(in)...};
  }(std::make_index_sequence<R::args::size>());
}

//
// Client side: from the argument values to the request
//

/// Number of trailing optional arguments, which callers may omit
template<typename Values>
struct trailing_optionals;

template<>
struct trailing_optionals<typelist<>> : std::integral_constant<std::size_t, 0> {};

template<typename V, typename... Vs>
struct trailing_optionals<typelist<V, Vs...>>
    : std::integral_constant<std::size_t, (trailing_optionals<typelist<Vs...>>::value == sizeof...(Vs) &&
                                           is_optional<V>::value)
                                              ? sizeof...(Vs) + 1
                                              : trailing_optionals<typelist<Vs...>>::value> {};

template<typename V, std::size_t I, typename Given>
V given_or_default(Given& given)
{
  if constexpr (I < std::tuple_size_v<Given>) {
    using A = std::tuple_element_t<I, Given>;
    static_assert(std::is_constructible_v<V, A>, "argument type does not match the endpoint");
    return V(std::forward<A>(std::get<I>(given)));
  } else {
    return V {};
  }
}

/// All the arguments of endpoint R from the ones given to call, omitted trailing optionals being empty
template<typename R, typename... A>
auto complete_args(A&&... args)
{
  using values = typename R::values;
  constexpr std::size_t required = values::size - trailing_optionals<values>::value;

  static_assert(sizeof...(A) <= values::size, "too many arguments for this endpoint");
  static_assert(sizeof...(A) >= required, "missing arguments for this endpoint");

  auto given = std::forward_as_tuple(std::forward<A>(args)...);

  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    return apply_t<std::tuple, values> {given_or_default<at_t<Is, values>, Is>(given)...};
  }(std::make_index_sequence<values::size>());
}

/// The parts of a request being built, the target is only valid once complete
struct outgoing {
  std::string target;
  webkit::headers headers;
  std::string body;
};

template<typename D, typename V>
void encode_arg(outgoing& out, const V& value)
{
  if constexpr (is_body<D>::value) {
    using codec = resolve_codec_t<V, typename D::codec>;
    out.body = codec::encode(value);
    out.headers.set("Content-Type", std::string(codec::content_type));
  } else if constexpr (is_header<D>::value) {
    format_text(value, [&](std::string text) { out.headers.set(std::string(D::name.view()), std::move(text)); });
  } else if constexpr (is_query<D>::value) {
    format_text(value, [&](const std::string& text) { append_query(out.target, D::name.view(), text); });
  }
  // Path parameters are formatted with the path
}

/// The request of endpoint R for these argument values
template<typename R, typename Values>
webkit::request encode_request(const Values& values, const webkit::headers& defaults)
{
  outgoing out;
  out.headers = defaults;

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    out.target = R::path_type::format(std::get<Is>(values)...);
  }(std::make_index_sequence<R::path_type::param_count>());

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (encode_arg<at_t<Is, typename R::args>>(out, std::get<Is>(values)), ...);
  }(std::make_index_sequence<R::args::size>());

  webkit::request req(std::string(method_name(R::method)), std::move(out.target));
  req.headers() = std::move(out.headers);
  req.body(std::move(out.body));
  return req;
}

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_ARGUMENTS_HPP
