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

#ifndef PUFFIN_REST_JSON_HPP
#define PUFFIN_REST_JSON_HPP

#include <puffin/rest/codec.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace puffin {
namespace rest {

/**
 * @brief JSON bodies with nlohmann::json: types need to_json / from_json, usually through
 *        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(type, fields...)
 */
struct json_codec {
  static constexpr std::string_view content_type = "application/json";

  template<typename T>
  static std::string encode(const T& value)
  {
    return nlohmann::json(value).dump();
  }

  template<typename T>
  static T decode(std::string_view text)
  {
    return nlohmann::json::parse(text.begin(), text.end()).template get<T>();
  }
};

/// JSON is the default codec of structured bodies once this header is included
template<>
struct default_structured_codec<void> {
  using type = json_codec;
};

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_JSON_HPP
