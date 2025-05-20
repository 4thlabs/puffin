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

#ifndef PUFFIN_IMDB_REQUEST_HPP
#define PUFFIN_IMDB_REQUEST_HPP

#include <algorithm>
#include <concepts>
#include <functional>
#include <puffin/imdb/imdb_traits.hpp>
#include <puffin/imdb/ranges/to_vector.hpp>
#include <ranges>

namespace puffin {
namespace imdb {

template<typename Key> Key to_key(uint64_t id);

template<>
inline std::string to_key(uint64_t id)
{
  return std::to_string(id);
}

template<typename Key, typename Value, typename Proj = Value, typename Traits = imdb_traits<Key, Value>>
class request
{
public:
  using fn_transform_type = std::function<Proj(const Value&)>;

  using fn_filter_type = Traits::fn_filter_type;
  using fn_order_type = Traits::fn_order_type;

  using key_type = Traits::key_type;
  using value_type = std::conditional_t
                                    <
                                      std::is_same<Value, Proj>::value,
                                      typename Traits::ref_value_type,
                                      Proj
                                    >;

  using view_type = Traits::view_type;
  using view_map = Traits::view_map;

  static constexpr auto filter_identity = [](const Value& v) constexpr { return true; };

  request()
    : fn_filter_(filter_identity)
  {}

  request(std::shared_ptr<view_map> v, fn_transform_type&& f)
      : views_(v)
      , fn_transform_(f),
        fn_filter_(filter_identity)
  {
    id_ = reinterpret_cast<uint64_t>(this);
  }

  request(std::shared_ptr<view_map> v)
      : request(v, nullptr)
  {}

  const key_type& from()
  {
    return from_;
  }

  request& from(const key_type& from)
  {
    from_ = from;
    return *this;
  }

  request& where(fn_filter_type&& f)
  {
    fn_filter_ = f;
    return *this;
  }

  request& order_by(fn_order_type&& f)
  {
    fn_order_ = f;
    return *this;
  }

  auto execute() -> std::vector<value_type>
  {
    view_type res {
      (*views_)[from_]
        | std::views::filter(fn_filter_)
        | puffin::ranges::to_vector()
    };

    if (fn_order_) {
      std::ranges::sort(res, fn_order_);
    }

    if constexpr (std::is_same<Value, Proj>::value) {
      return res;
    } else {
      auto t = res | std::views::transform(fn_transform_);
      return std::vector<Proj>(t.begin(), t.end());
    }
  }

  template<typename C>
  auto execute(C& views) {
    view_type res {
      views[from_]
        | std::views::filter(fn_filter_)
        | puffin::ranges::to_vector()
    };

    if (fn_order_) {
      std::ranges::sort(res, fn_order_);
    }

    if constexpr (std::is_same<Value, Proj>::value) {
      return res;
    } else {
      auto t = res | std::views::transform(fn_transform_);
      return std::vector<Proj>(t.begin(), t.end());
    }
  }


private:
  uint64_t id_;

  std::shared_ptr<view_map> views_;

  fn_transform_type fn_transform_;
  fn_filter_type fn_filter_;
  fn_order_type fn_order_ = nullptr;

  key_type from_;
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_REQUEST_HPP
