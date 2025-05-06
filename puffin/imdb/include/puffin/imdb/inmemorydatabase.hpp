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

#ifndef PUFFIN_IMDB_INMEMORYDATABASE_HPP
#define PUFFIN_IMDB_INMEMORYDATABASE_HPP

#include <map>

namespace puffin {
namespace imdb {

template<typename Key, typename Value, typename Proj, typename Container, typename View>
class request;

/**
 * @brief A small in memory database
 */
template <typename Key,
          typename Value,
          template <typename, typename, typename...> typename Container = std::map>
class basic_in_memory_database {
public:
  using key_type = Key;
  using value_type = Value;
  using value_ref_type = std::reference_wrapper<value_type>;

  using container_type = Container<key_type, std::vector<value_type>>;
  using view_type = Container<key_type, std::vector<value_ref_type>>;

  template<typename P>
  using request_type = request<key_type, value_type, P, container_type, view_type>;

  static constexpr auto fn_identity = [](const Value& v) { return v; };

  basic_in_memory_database()
      : data_(std::make_shared<container_type>())
      , data_views_(std::make_shared<view_type>())
  {}
  ~basic_in_memory_database() {}

  void insert(const key_type &key, const value_type &value)
  {
    (*data_)[key].push_back(value);
  }

  std::size_t size(const key_type &key)
  {
    return (*data_)[key].size();
  }

  const value_type& row(const key_type& key, int index)
  {
    return (*data_)[key].at(index);
  }

  const value_type &data(const key_type &key)
  {
    return (*data_)[key];
  }

  auto select() -> request_type<value_type>
  {
    return request_type<value_type>(data_, data_views_);
  }

  template<typename F>
  auto select(F&& f) -> request_type<decltype(std::declval<F>()(std::declval<Value>()))>
  {
    return request_type<decltype(std::declval<F>()(std::declval<Value>()))>(data_, data_views_, std::forward<F>(f));
  }

private:
  std::shared_ptr<view_type> data_views_;
  std::shared_ptr<container_type> data_;
};

template <typename Key, typename Value>
using in_memory_database = basic_in_memory_database<Key, Value>;

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_INMEMORYDATABASE_HPP
