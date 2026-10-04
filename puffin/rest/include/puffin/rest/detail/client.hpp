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
#include <puffin/rest/detail/arguments.hpp>
#include <puffin/rest/detail/typelist.hpp>
#include <puffin/rest/dsl.hpp>
#include <puffin/rest/errors.hpp>
#include <puffin/rest/params.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {
namespace detail {

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

/// One address per security scheme type, identifying it among the credentials of a client
template<typename S>
inline constexpr char scheme_key = 0;

/// Sends the credential of scheme S where the scheme says
template<typename S>
void add_credential(outgoing& out, const std::string& value)
{
  if constexpr (std::is_same_v<S, bearer_auth>)
    out.headers.set("Authorization", param_traits<bearer>::format(bearer {value}));
  else if constexpr (S::in_query)
    append_query(out.target, S::name.view(), value);
  else
    out.headers.set(std::string(S::name.view()), value);
}

/// The credentials of a client, sent for the security schemes of the called endpoints
class credential_store {
public:
  template<typename S>
  void set(std::string value)
  {
    values_[&scheme_key<S>] = std::move(value);
  }

  /// Adds the credentials the schemes of endpoint R need, when known
  template<typename R>
  void apply(outgoing& out) const
  {
    [&]<typename... S>(typelist<S...>) { (apply_one<S>(out), ...); }(typename R::schemes {});
  }

private:
  template<typename S>
  void apply_one(outgoing& out) const
  {
    auto it = values_.find(&scheme_key<S>);

    if (it != values_.end())
      add_credential<S>(out, it->second);
  }

  std::map<const char*, std::string> values_; ///< By scheme_key: two schemes never share a credential
};

} // namespace detail
} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_DETAIL_CLIENT_HPP
