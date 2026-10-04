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

#ifndef PUFFIN_REST_CODEC_HPP
#define PUFFIN_REST_CODEC_HPP

#include <puffin/webkit/detail/string.hpp>

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace puffin {
namespace rest {

/**
 * @brief Encodes and decodes request and response bodies of type T.
 *
 * Decoding failures throw (any exception): the server answers 400, the client rethrows.
 */
template<typename C, typename T>
concept Codec = requires(const T& value, std::string_view text) {
  { C::content_type } -> std::convertible_to<std::string_view>;
  { C::encode(value) } -> std::convertible_to<std::string>;
  { C::template decode<T>(text) } -> std::same_as<T>;
};

/// Plain text bodies, for std::string
struct text_codec {
  static constexpr std::string_view content_type = "text/plain; charset=utf-8";

  static std::string encode(const std::string& value) { return value; }

  template<typename T>
    requires std::same_as<T, std::string>
  static T decode(std::string_view text)
  {
    return std::string(text);
  }
};

/**
 * @brief The codec of structured bodies when none is given to body<> or returns<>.
 *
 * Declared only: including <puffin/rest/json.hpp> defines it as the JSON codec. Another default can be
 * installed by defining this specialization instead.
 */
template<typename Void>
struct default_structured_codec;

/// Marker for "the default codec of the type": text_codec for std::string, default_structured_codec otherwise
struct default_codec {};

namespace detail {

template<typename T, typename C>
struct resolve_codec {
  using type = C;
};

template<typename T>
struct resolve_codec<T, default_codec> {
  // Dependent on T so that the structured codec is only looked up when used
  using type = std::conditional_t<std::is_same_v<T, std::string>, text_codec,
                                  typename default_structured_codec<std::enable_if_t<sizeof(T) != 0>>::type>;
};

template<typename T, typename C>
using resolve_codec_t = typename resolve_codec<T, C>::type;

/// The codec of the result of a resolved endpoint, when it returns a value
template<typename R>
using result_codec_t = resolve_codec_t<typename R::result_type, typename R::returns::codec>;

/// Compares the media types of two Content-Type values, ignoring parameters (charset...)
inline bool same_media_type(std::string_view a, std::string_view b)
{
  auto media = [](std::string_view s) { return webkit::detail::trim(s.substr(0, s.find(';'))); };

  return webkit::detail::iequals(media(a), media(b));
}

} // namespace detail

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_CODEC_HPP
