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

#ifndef PUFFIN_REST_DSL_HPP
#define PUFFIN_REST_DSL_HPP

#include <puffin/rest/codec.hpp>
#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/fixed_string.hpp>
#include <puffin/rest/params.hpp>
#include <puffin/rest/path.hpp>
#include <puffin/webkit/http/status.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

// windows.h defines DELETE
#ifdef DELETE
#pragma push_macro("DELETE")
#undef DELETE
#define PUFFIN_REST_RESTORE_DELETE
#endif

namespace puffin {
namespace rest {

using webkit::status;

enum class http_method { get, head, post, put, patch, del, options };

inline constexpr http_method GET = http_method::get;
inline constexpr http_method HEAD = http_method::head;
inline constexpr http_method POST = http_method::post;
inline constexpr http_method PUT = http_method::put;
inline constexpr http_method PATCH = http_method::patch;
inline constexpr http_method DELETE = http_method::del;
inline constexpr http_method DEL = http_method::del; ///< Where DELETE is a macro (windows.h)
inline constexpr http_method OPTIONS = http_method::options;

constexpr std::string_view method_name(http_method m) noexcept
{
  switch (m) {
    case http_method::get: return "GET";
    case http_method::head: return "HEAD";
    case http_method::post: return "POST";
    case http_method::put: return "PUT";
    case http_method::patch: return "PATCH";
    case http_method::del: return "DELETE";
    case http_method::options: return "OPTIONS";
  }

  return "GET";
}

/// True for methods a client may safely send twice
constexpr bool idempotent(http_method m) noexcept { return m != http_method::post && m != http_method::patch; }

namespace detail {

struct part_tag {};
struct arg_tag {};
struct scheme_tag {};
struct endpoint_tag {};
struct api_tag {};
struct with_tag {};

} // namespace detail

//
// Endpoint parts
//

/// A query parameter. optional<T>: may be absent, vector<T>: repeated (?tag=a&tag=b)
template<fixed_string Name, typename T>
struct query : detail::part_tag, detail::arg_tag {
  static_assert(Param<detail::param_element_t<T>>, "query parameter type without param_traits");

  static constexpr auto name = Name;
  using value_type = T;
};

/// A request header. optional<T>: may be absent
template<fixed_string Name, typename T>
struct header : detail::part_tag, detail::arg_tag {
  static_assert(!detail::is_vector<T>::value, "vector headers are not supported");
  static_assert(Param<detail::param_element_t<T>>, "header type without param_traits");

  static constexpr auto name = Name;
  using value_type = T;
};

/// The request body, decoded with Codec (JSON when <puffin/rest/json.hpp> is included)
template<typename T, typename C = default_codec>
struct body : detail::part_tag, detail::arg_tag {
  using value_type = T;
  using codec = C;
};

/// The response on success: body type, status and codec. returns<void> (or no returns at all) answers 204.
template<typename T = void, webkit::status S = std::is_void_v<T> ? webkit::status::no_content : webkit::status::ok,
         typename C = default_codec>
struct returns : detail::part_tag {
  using value_type = T;
  static constexpr webkit::status status = S;
  using codec = C;
};

//
// Security schemes, checked by the server validators and filled by the client interceptors. They are not
// part of the handler and call signatures.
//

/// An API key sent in a header: api_key<"X-Api-Key">
template<fixed_string Header>
struct api_key : detail::scheme_tag {
  static constexpr auto name = Header;
  static constexpr bool in_query = false;
};

/// An API key sent as a query parameter: api_key_query<"api_key">
template<fixed_string Param>
struct api_key_query : detail::scheme_tag {
  static constexpr auto name = Param;
  static constexpr bool in_query = true;
};

/// A bearer token in the Authorization header
struct bearer_auth : detail::scheme_tag {};

/// Requires every scheme listed
template<typename... Schemes>
struct security : detail::part_tag {
  static_assert((std::is_base_of_v<detail::scheme_tag, Schemes> && ...), "security<> takes security schemes");

  using schemes = detail::typelist<Schemes...>;
};

namespace detail {

template<typename T>
struct is_part : std::is_base_of<part_tag, T> {};

template<typename T>
struct is_not_part : std::bool_constant<!is_part<T>::value> {};

template<typename T>
struct is_arg : std::is_base_of<arg_tag, T> {};

template<typename T>
struct is_returns : std::false_type {};

template<typename T, webkit::status S, typename C>
struct is_returns<returns<T, S, C>> : std::true_type {};

template<typename T>
struct is_body : std::false_type {};

template<typename T, typename C>
struct is_body<body<T, C>> : std::true_type {};

template<typename T>
struct is_security : std::false_type {};

template<typename... S>
struct is_security<security<S...>> : std::true_type {};

template<typename T>
struct value_type_of {
  using type = typename T::value_type;
};

template<typename List>
struct schemes_of;

template<typename... Securities>
struct schemes_of<typelist<Securities...>> {
  using type = concat_t<typename Securities::schemes...>;
};

template<typename List, typename Default>
struct first_or {
  using type = Default;
};

template<typename T, typename... Ts, typename Default>
struct first_or<typelist<T, Ts...>, Default> {
  using type = T;
};

} // namespace detail

/**
 * @brief An endpoint: name, method, path template relative to its api, and parts (query, header, body,
 *        returns, security).
 *
 *   using get = endpoint<"users.get", GET, "/{id:int}", returns<user>>;
 *
 * The name makes the endpoint type unique, the type being its identity (service dispatch, client::call): two
 * endpoints with the same method and path in different domains stay distinct. Names must be unique in an api,
 * checked at compile time. They are also given to the interceptors (call_info::name).
 */
template<fixed_string Name, http_method M, fixed_string Path, typename... Parts>
struct endpoint : detail::endpoint_tag {
  static_assert(Name.size() > 0, "endpoint name must not be empty");

  static_assert((detail::is_part<Parts>::value && ...),
                "endpoint parts must be query<>, header<>, body<>, returns<> or security<>");
  static_assert(detail::filter_t<detail::is_body, detail::typelist<Parts...>>::size <= 1, "more than one body<>");
  static_assert(detail::filter_t<detail::is_returns, detail::typelist<Parts...>>::size <= 1,
                "more than one returns<>");

  static constexpr auto name = Name;
  static constexpr http_method method = M;
  static constexpr auto path = Path;
  using parts = detail::typelist<Parts...>;
};

/// Parts (headers, query parameters, security) added to every endpoint of the children: with<security<...>, users_api>
template<typename... Ts>
struct with : detail::with_tag {
  using parts = detail::filter_t<detail::is_part, detail::typelist<Ts...>>;
  using children = detail::filter_t<detail::is_not_part, detail::typelist<Ts...>>;

  static_assert(detail::filter_t<detail::is_returns, parts>::size == 0, "returns<> cannot be shared with with<>");
  static_assert(detail::filter_t<detail::is_body, parts>::size == 0, "body<> cannot be shared with with<>");
};

/// A path parameter, the I-th of the full path of an endpoint
template<fixed_string Path, std::size_t I>
struct path_arg : detail::arg_tag {
  using template_type = path_template<Path>;
  using value_type = typename path_template<Path>::template param_type<I>;
  static constexpr std::size_t index = I;
  static constexpr std::string_view name = path_template<Path>::param_name(I);
};

/**
 * @brief An endpoint placed in an api: its full path and the parts inherited from with<>.
 *
 * Arguments of the handlers and of client::call, in order: path parameters, then the endpoint parts in
 * declaration order, then the inherited parts.
 */
template<typename E, fixed_string Path, typename Inherited>
struct resolved {
  using endpoint = E;
  static constexpr http_method method = E::method;
  static constexpr auto path = Path;
  using path_type = path_template<Path>;

  using parts = detail::concat_t<typename E::parts, Inherited>;
  using returns = typename detail::first_or<detail::filter_t<detail::is_returns, typename E::parts>, rest::returns<>>::type;
  using result_type = typename returns::value_type;
  static constexpr std::string_view name = E::name.view();
  using schemes = typename detail::schemes_of<detail::filter_t<detail::is_security, parts>>::type;

private:
  template<std::size_t... Is>
  static auto make_path_args(std::index_sequence<Is...>) -> detail::typelist<path_arg<Path, Is>...>;

public:
  /// Argument descriptors: path_arg, query, header, body
  using args = detail::concat_t<decltype(make_path_args(std::make_index_sequence<path_type::param_count>())),
                                detail::filter_t<detail::is_arg, parts>>;

  /// Argument types
  using values = detail::transform_t<detail::value_type_of, args>;
};

namespace detail {

template<fixed_string Prefix, typename Inherited, typename Node, typename Kind = void>
struct flatten;

template<fixed_string Prefix, typename Inherited, typename Children>
struct flatten_children;

template<fixed_string Prefix, typename Inherited, typename... Children>
struct flatten_children<Prefix, Inherited, typelist<Children...>> {
  using type = concat_t<typename flatten<Prefix, Inherited, Children>::type...>;
};

template<fixed_string Prefix, typename Inherited, typename Node>
struct flatten<Prefix, Inherited, Node, std::enable_if_t<std::is_base_of_v<endpoint_tag, Node>>> {
  using type = typelist<resolved<Node, join_path<Prefix, Node::path>, Inherited>>;
};

template<fixed_string Prefix, typename Inherited, typename Node>
struct flatten<Prefix, Inherited, Node, std::enable_if_t<std::is_base_of_v<api_tag, Node>>> {
  using type = typename flatten_children<join_path<Prefix, Node::prefix>, Inherited, typename Node::children>::type;
};

template<fixed_string Prefix, typename Inherited, typename Node>
struct flatten<Prefix, Inherited, Node, std::enable_if_t<std::is_base_of_v<with_tag, Node>>> {
  using type = typename flatten_children<Prefix, concat_t<Inherited, typename Node::parts>, typename Node::children>::type;
};

template<typename E>
struct is_resolved_for {
  template<typename R>
  struct pred : std::is_same<typename R::endpoint, E> {};
};

} // namespace detail

/**
 * @brief A group of endpoints under a path prefix. Children are endpoints, other api<> and with<>.
 *
 *   using v1 = api<"/api/v1", auth::api, with<security<api_key<"X-Api-Key">>, users::api, cards::api>>;
 */
template<fixed_string Prefix, typename... Children>
struct api : detail::api_tag {
  static constexpr auto prefix = Prefix;
  using children = detail::typelist<Children...>;

  /// Every endpoint of the api, as resolved<>
  using endpoints = typename detail::flatten_children<join_path<"", Prefix>, detail::typelist<>, children>::type;

  template<typename E>
  static constexpr std::size_t count = detail::filter_t<detail::is_resolved_for<E>::template pred, endpoints>::size;

  template<typename E>
  static constexpr bool contains = count<E> > 0;

private:
  template<typename... Rs>
  static constexpr bool unique_names(detail::typelist<Rs...>)
  {
    constexpr std::string_view names[] = {std::string_view(), Rs::name...};

    for (std::size_t i = 1; i < sizeof(names) / sizeof(names[0]); ++i) {
      for (std::size_t j = 1; j < i; ++j) {
        if (names[i] == names[j])
          return false;
      }
    }

    return true;
  }

  static_assert(unique_names(endpoints {}), "two endpoints of the api have the same name");
};

/// The resolved<> of endpoint E in Api
template<typename Api, typename E>
struct find_endpoint {
  static_assert(Api::template count<E> != 0, "this endpoint is not part of the api");
  static_assert(Api::template count<E> < 2, "this endpoint appears several times in the api");

  using type = detail::at_t<0, detail::filter_t<detail::is_resolved_for<E>::template pred, typename Api::endpoints>>;
};

template<typename Api, typename E>
using find_endpoint_t = typename find_endpoint<Api, E>::type;

/**
 * @brief Static information on the endpoint being called, given to the interceptors
 */
template<typename R>
struct call_info {
  using endpoint = typename R::endpoint;
  using resolved = R;
  using schemes = typename R::schemes;

  static constexpr http_method method = R::method;
  static constexpr std::string_view method_name = rest::method_name(R::method);
  static constexpr std::string_view path_template = R::path.view();
  static constexpr std::string_view name = R::name;

  template<typename Scheme>
  static constexpr bool requires_scheme = detail::contains_v<Scheme, schemes>;
};

} // namespace rest
} // namespace puffin

#ifdef PUFFIN_REST_RESTORE_DELETE
#pragma pop_macro("DELETE")
#undef PUFFIN_REST_RESTORE_DELETE
#endif

#endif // PUFFIN_REST_DSL_HPP
