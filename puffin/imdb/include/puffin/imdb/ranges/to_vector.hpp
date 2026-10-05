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

#ifndef PUFFIN_RANGES_TO_VECTOR_HPP
#define PUFFIN_RANGES_TO_VECTOR_HPP

#include <ranges>

namespace puffin {
namespace ranges {

/**
 * @brief A to_vector implementation (C++23 alike)
 */
struct to_vector_impl {

  struct closure {
    template<std::ranges::range R>
    constexpr auto operator()(R&& r) const {
      auto rc = r | std::views::common;
      std::vector<std::ranges::range_value_t<R>> res;

      if constexpr (requires { std::ranges::size(r); }) {
        res.reserve(std::ranges::size(r));
      }

      res.insert(res.begin(), rc.begin(), rc.end());

      return res;
    }
  };

  auto operator()() const -> closure
  {
    return closure{};
  }
};

inline constexpr auto to_vector = to_vector_impl{};

template<std::ranges::range R>
auto constexpr operator|(R&& r, const to_vector_impl::closure& c)
{
  return c(std::forward<R>(r));
}

} // namespace ranges
} // namespace puffin

#endif // PUFFIN_RANGES_TO_VECTOR_HPP
