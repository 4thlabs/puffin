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

#ifndef PUFFIN_REST_FIXED_STRING_HPP
#define PUFFIN_REST_FIXED_STRING_HPP

#include <cstddef>
#include <string_view>

namespace puffin {
namespace rest {

/**
 * @brief A string usable as a template argument: endpoint<GET, "/users/{id:int}">, query<"limit", int>...
 */
template<std::size_t N>
struct fixed_string {
  char data[N] {}; // With the terminating null

  constexpr fixed_string() = default;

  constexpr fixed_string(const char (&s)[N])
  {
    for (std::size_t i = 0; i < N; ++i)
      data[i] = s[i];
  }

  static constexpr std::size_t size() noexcept { return N - 1; }

  constexpr std::string_view view() const noexcept { return std::string_view(data, N - 1); }
  constexpr operator std::string_view() const noexcept { return view(); }

  constexpr char operator[](std::size_t i) const noexcept { return data[i]; }

  template<std::size_t M>
  constexpr bool operator==(const fixed_string<M>& rhs) const noexcept
  {
    return view() == rhs.view();
  }
};

template<std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

namespace detail {

template<std::size_t Capacity>
struct path_buffer {
  char data[Capacity] {};
  std::size_t size = 0;

  constexpr void push(char c)
  {
    // Collapses the "//" appearing when joining "/a/" and "/b"
    if (c == '/' && size > 0 && data[size - 1] == '/')
      return;

    data[size++] = c;
  }
};

template<fixed_string A, fixed_string B>
constexpr auto join_paths()
{
  path_buffer<A.size() + B.size() + 2> buffer;

  if (A.size() == 0 || A[0] != '/')
    buffer.push('/');

  for (std::size_t i = 0; i < A.size(); ++i)
    buffer.push(A[i]);

  if (B.size() > 0 && B[0] != '/')
    buffer.push('/');

  for (std::size_t i = 0; i < B.size(); ++i)
    buffer.push(B[i]);

  // No trailing slash, except for the root
  if (buffer.size > 1 && buffer.data[buffer.size - 1] == '/')
    --buffer.size;

  return buffer;
}

} // namespace detail

/// Joins two path templates: "/api/v1" + "/users/" gives "/api/v1/users"
template<fixed_string A, fixed_string B>
inline constexpr auto join_path = [] {
  constexpr auto buffer = detail::join_paths<A, B>();
  fixed_string<buffer.size + 1> result;

  for (std::size_t i = 0; i < buffer.size; ++i)
    result.data[i] = buffer.data[i];

  return result;
}();

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_FIXED_STRING_HPP
