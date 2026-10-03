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

#ifndef PUFFIN_IMDB_NLOHMANN_HPP
#define PUFFIN_IMDB_NLOHMANN_HPP

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace puffin {
namespace imdb {

template<typename Key>
struct json_sort {
  Key key;

  bool operator()(const json& v1, const json& v2) {
    return (v1.contains(key) && v2.contains(key)) ? v1[key] < v2[key] : false;
  }
};

template<typename Key>
struct json_contains {
  Key key;
  std::string text = "";

  bool operator()(const json& v1) {
    if (!v1.contains(key))
      return false;

    return std::find_if(v1[key].begin(), v1[key].end(), [&](const json& j) {
      return j.get<std::string>().compare(text) == 0;
    }) != std::end(v1[key]);
  }
};

template<typename Key>
struct json_exists {
    Key key;

    bool operator()(const json& v) {
        return v.contains(key);
    }
};

template<typename Key>
struct json_search {
  Key key;
  std::string text = "";

  bool operator()(const json& v1) {
    return v1.contains(key) ? v1[key].template get<std::string>().find(text) != std::string::npos : false;
  }
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_NLOHMANN_HPP
