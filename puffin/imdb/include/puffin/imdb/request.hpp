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

#include <functional>
#include <ranges>

#include <nlohmann/json.hpp>

namespace puffin {
namespace imdb {

template<typename Key> Key to_key(uint64_t id);

template<>
std::string to_key(uint64_t id)
{
  return std::to_string(id);
}

template<typename Key, typename Value, typename Proj, typename Container, typename View>
class request
{
public:
  using transform_fn_type = std::function<Proj(const Value&)>;
  using filter_fn_type = std::function<bool(const Value&)>;
  using order_fn_type = std::function<bool(const Value&, const Value&)>;

  using value_type = std::conditional<std::is_same<Value, Proj>::value, std::reference_wrapper<Value>, Proj>::type;

  static constexpr auto transform_identity = [](const Value& v) constexpr { return v;};
  static constexpr auto filter_identity = [](const Value& v) constexpr { return true; };

  request(std::shared_ptr<Container> d, std::shared_ptr<View> v, transform_fn_type&& f)
      : data_(d), views_(v)
      , fn_transform_(std::forward<transform_fn_type>(f)),
        fn_filter_(filter_identity)
  {
    id_ = reinterpret_cast<uint64_t>(this);
  }

  request(std::shared_ptr<Container> d, std::shared_ptr<View> v)
      : request(d, v, nullptr)
  {}

  request& from(const Key& from)
  {
    from_ = from;
    return *this;
  }

  request& where(filter_fn_type&& f)
  {
    fn_filter_ = f;
    return *this;
  }

  request& order_by(order_fn_type&& f)
  {
    fn_order_ = f;
    return *this;
  }

  auto execute() -> std::vector<value_type>
  {
    auto r = (*data_)[from_] | std::views::filter(fn_filter_);

    std::vector<std::reference_wrapper<Value>> res(r.begin(), r.end());

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

  std::shared_ptr<Container> data_;
  std::shared_ptr<View> views_;

  transform_fn_type fn_transform_;
  filter_fn_type fn_filter_;
  order_fn_type fn_order_ = nullptr;

  Key from_;
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_REQUEST_HPP
