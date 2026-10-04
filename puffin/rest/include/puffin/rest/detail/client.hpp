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

#ifndef PUFFIN_REST_DETAIL_CLIENT_HPP
#define PUFFIN_REST_DETAIL_CLIENT_HPP

#include <puffin/async/try_await.hpp>
#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/webkit/client/client.hpp>

#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {
namespace detail {

template<typename T>
struct transport_for {
  using type = T;
};

template<webkit::Connector C>
struct transport_for<C> {
  using type = webkit::basic_client<C>;
};

/// Statuses worth sending an idempotent request again for
constexpr bool retryable_status(int status) noexcept { return status == 502 || status == 503 || status == 504; }

/// The result of a call from its outcome: http_error becomes an error value, other exceptions are rethrown
template<typename T, typename V>
result<T> to_result(async::outcome<V> outcome)
{
  if (outcome) {
    if constexpr (std::is_void_v<T>)
      return result<void>();
    else
      return result<T>(std::move(*outcome.value));
  }

  return result<T>(http_error_of(outcome));
}

template<typename Schemes>
struct for_each_scheme;

template<typename... S>
struct for_each_scheme<typelist<S...>> {
  template<typename F>
  static void apply(F&& f)
  {
    (f.template operator()<S>(), ...);
  }
};

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_CLIENT_HPP
