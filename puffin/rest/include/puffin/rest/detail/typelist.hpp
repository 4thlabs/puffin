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

#ifndef PUFFIN_REST_DETAIL_TYPELIST_HPP
#define PUFFIN_REST_DETAIL_TYPELIST_HPP

#include <array>
#include <cstddef>
#include <type_traits>

namespace puffin {
namespace rest {
namespace detail {

template<typename... Ts>
struct typelist {
  static constexpr std::size_t size = sizeof...(Ts);
};

template<typename... Lists>
struct concat;

template<>
struct concat<> {
  using type = typelist<>;
};

template<typename... Ts>
struct concat<typelist<Ts...>> {
  using type = typelist<Ts...>;
};

template<typename... As, typename... Bs, typename... Rest>
struct concat<typelist<As...>, typelist<Bs...>, Rest...> : concat<typelist<As..., Bs...>, Rest...> {};

template<typename... Lists>
using concat_t = typename concat<Lists...>::type;

/// Keeps the types satisfying Pred
template<template<typename> class Pred, typename List>
struct filter;

template<template<typename> class Pred, typename... Ts>
struct filter<Pred, typelist<Ts...>> {
  using type = concat_t<std::conditional_t<Pred<Ts>::value, typelist<Ts>, typelist<>>...>;
};

template<template<typename> class Pred, typename List>
using filter_t = typename filter<Pred, List>::type;

/// Applies F to every type
template<template<typename> class F, typename List>
struct transform;

template<template<typename> class F, typename... Ts>
struct transform<F, typelist<Ts...>> {
  using type = typelist<typename F<Ts>::type...>;
};

template<template<typename> class F, typename List>
using transform_t = typename transform<F, List>::type;

/// Rebinds the types of a list to another template: apply_t<std::tuple, typelist<int, bool>>
template<template<typename...> class T, typename List>
struct apply;

template<template<typename...> class T, typename... Ts>
struct apply<T, typelist<Ts...>> {
  using type = T<Ts...>;
};

template<template<typename...> class T, typename List>
using apply_t = typename apply<T, List>::type;

template<std::size_t I, typename List>
struct at;

template<std::size_t I, typename T, typename... Ts>
struct at<I, typelist<T, Ts...>> : at<I - 1, typelist<Ts...>> {};

template<typename T, typename... Ts>
struct at<0, typelist<T, Ts...>> {
  using type = T;
};

template<std::size_t I, typename List>
using at_t = typename at<I, List>::type;

template<typename T, typename List>
struct contains;

template<typename T, typename... Ts>
struct contains<T, typelist<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<typename T, typename List>
inline constexpr bool contains_v = contains<T, List>::value;

/// Index of the first true flag, N if none
template<std::size_t N>
constexpr std::size_t first_true(const std::array<bool, N>& a)
{
  for (std::size_t i = 0; i < N; ++i) {
    if (a[i])
      return i;
  }

  return N;
}

/// Number of true flags
template<std::size_t N>
constexpr std::size_t count_true(const std::array<bool, N>& a)
{
  std::size_t n = 0;

  for (bool b : a)
    n += b;

  return n;
}

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_TYPELIST_HPP
