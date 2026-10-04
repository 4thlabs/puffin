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

#ifndef PUFFIN_WEBKIT_HTTP_HEADERS_HPP
#define PUFFIN_WEBKIT_HTTP_HEADERS_HPP

#include <puffin/webkit/detail/string.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {

/**
 * @brief A single header field
 */
struct header {
  std::string name;
  std::string value;
};

/**
 * @brief Ordered collection of header fields.
 *
 * Names are compared case insensitively. A name can appear several times (Set-Cookie for example), set() replaces
 * every occurrence while add() appends a new one.
 */
class headers {
public:
  using container_type = std::vector<header>;
  using const_iterator = container_type::const_iterator;

  headers() = default;

  headers(std::initializer_list<header> fields)
  {
    for (const auto& f : fields)
      add(f.name, f.value);
  }

  /// Appends a field, keeping existing fields with the same name.
  /// Throws std::invalid_argument if the name is not a token or the value contains CR, LF, NUL or other controls.
  void add(std::string name, std::string value)
  {
    validate(name, value);
    fields_.push_back({std::move(name), std::move(value)});
  }

  /// Replaces every field with this name by a single one
  void set(std::string name, std::string value)
  {
    validate(name, value);
    erase(name);
    add(std::move(name), std::move(value));
  }

  /// Removes every field with this name, returns the number of removed fields
  std::size_t erase(std::string_view name)
  {
    auto it = std::remove_if(fields_.begin(), fields_.end(), [&](const header& h) {
      return detail::iequals(h.name, name);
    });

    std::size_t count = static_cast<std::size_t>(fields_.end() - it);
    fields_.erase(it, fields_.end());
    return count;
  }

  /// Returns the value of the first field with this name
  std::optional<std::string_view> get(std::string_view name) const
  {
    for (const auto& h : fields_) {
      if (detail::iequals(h.name, name))
        return std::string_view(h.value);
    }

    return std::nullopt;
  }

  /// Returns the values of every field with this name
  std::vector<std::string_view> get_all(std::string_view name) const
  {
    std::vector<std::string_view> values;

    for (const auto& h : fields_) {
      if (detail::iequals(h.name, name))
        values.emplace_back(h.value);
    }

    return values;
  }

  bool contains(std::string_view name) const { return get(name).has_value(); }

  /// True if a comma separated header contains the token (e.g. Connection: keep-alive, Upgrade)
  bool has_token(std::string_view name, std::string_view token) const
  {
    for (const auto& h : fields_) {
      if (!detail::iequals(h.name, name))
        continue;

      std::string_view value = h.value;

      while (!value.empty()) {
        auto comma = value.find(',');
        auto item = detail::trim(value.substr(0, comma));

        if (detail::iequals(item, token))
          return true;

        if (comma == std::string_view::npos)
          break;

        value.remove_prefix(comma + 1);
      }
    }

    return false;
  }

  std::size_t size() const noexcept { return fields_.size(); }
  bool empty() const noexcept { return fields_.empty(); }
  void clear() noexcept { fields_.clear(); }

  const_iterator begin() const noexcept { return fields_.begin(); }
  const_iterator end() const noexcept { return fields_.end(); }

private:
  static void validate(std::string_view name, std::string_view value)
  {
    if (!detail::is_token(name))
      throw std::invalid_argument("invalid header name");

    if (!detail::is_field_value(value))
      throw std::invalid_argument("invalid header value");
  }

  container_type fields_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_HEADERS_HPP
