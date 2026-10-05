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

#include <puffin/imdb/imdb_traits.hpp>
#include <list>
#include <map>

namespace puffin {
namespace imdb {

template<typename Key, typename Value, typename Proj, typename Traits>
class request;

/**
 * @brief A small in memory database
 */
template <typename Key,
          typename Value,
          typename Traits = imdb_traits<Key, Value>>
class basic_in_memory_database
{
public:
  using key_type   = Traits::key_type;
  using value_type = Traits::value_type;

  using ref_value_type = Traits::ref_value_type;

  using data_type = Traits::data_type;
  using view_type = Traits::view_type;

  using view_map = Traits::view_map;

  using index_map = Traits::index_map;

  template<typename P>
  using request_type = request<key_type, value_type, P, Traits>;

  /**
   * The request map accepts only for now request without transformations
   */
  using request_map = std::map<key_type, std::list<request_type<value_type>>>;

  basic_in_memory_database()
      : data_()
      , data_views_(std::make_shared<view_map>())
      , indexes_(std::make_shared<index_map>())
  {}

  ~basic_in_memory_database() {}

  basic_in_memory_database(const basic_in_memory_database&) = delete;
  basic_in_memory_database& operator_(const basic_in_memory_database&) = delete;

  /**
   * @brief Creates a view based on the the provided request
   * @param key
   * @param req
   */
  void create_view(const key_type& key, request_type<value_type>&& req)
  {
    view_request_[key].push_back(req);
    (*data_views_)[key] = req.execute();
  }

  /**
   * @brief Crates an index based on a function
   * @param table - The data table key
   * @param index - the index key ( Name )
   * @param f - Function that returns a list of keys for a specific object
   */
  template<typename F>
  void create_index(const key_type& table, const key_type& index, F&& f)
  {
    for (auto& value : (*data_views_)[table]) {
      auto list = f(value);

      for (auto& i : list) {
        (*indexes_)[index].insert({i, value});
      }
    }
  }

  ref_value_type insert(const key_type &key, const value_type &value)
  {
    data_.push_back(value);
    (*data_views_)[key].push_back(data_.back());

    return (*data_views_)[key].back();
  }

  template<typename Iterator>
  void batch_insert(const key_type &key, Iterator begin, Iterator end)
  {
    auto it = data_.end();
    it--;
    std::copy(begin, end, std::back_inserter(data_));
    std::copy(++it, data_.end(), std::back_inserter((*data_views_)[key]));
  }

  std::size_t size(const key_type &key)
  {
    return (*data_views_)[key].size();
  }

  /**
   * @brief Returns a row from a table based on it's indec
   * @param key - The table key
   * @param index - the row numeric index
   * @return value_type&
   */
  value_type& row(const key_type& key, int index)
  {
    return (*data_views_)[key].at(index).get();
  }

  /**
   * @brief This version of row gets a value from an index, can throws
   * @param key
   * @param index
   * @return
   */
  value_type& row(const key_type& key, const key_type& index)
  {
    return (*indexes_)[key].at(index).get();
  }

  view_type &data(const key_type &key)
  {
    return (*data_views_)[key];
  }

  auto select() const -> request_type<value_type>
  {
    return request_type<value_type>(data_views_);
  }

  template<typename F>
  auto select(F&& f) const -> request_type<decltype(std::declval<F>()(std::declval<Value>()))>
  {
    return request_type<decltype(std::declval<F>()(std::declval<Value>()))>(data_views_, std::forward<F>(f));
  }

  template<typename Q>
  auto query(Q query) {
    return query.execute((*data_views_));
  }

  /**
   * @brief Updates all views based on the data from key
   * @param key
   */
  void update_views(const key_type& key)
  {
    for (auto& req : view_request_[key]) {
      (*data_views_)[key] = req.execute();
    }
  }

private:
  std::shared_ptr<view_map> data_views_;
  std::shared_ptr<index_map> indexes_;

  request_map view_request_;

  data_type data_;
};

template <typename Key, typename Value>
using in_memory_database = basic_in_memory_database<Key, Value>;

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_INMEMORYDATABASE_HPP
