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

#ifndef PUFFIN_WEBKIT_SERVER_ROUTER_HPP
#define PUFFIN_WEBKIT_SERVER_ROUTER_HPP

#include <algorithm>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {

/**
 * @brief Routes requests to handlers from their method and path.
 *
 * Patterns are regular expressions matching the whole path, capture groups become route parameters:
 * "/users/([0-9]+)". Routes are tried in insertion order. The handler type is left to the server, so the router
 * does not depend on how handlers are invoked (plain functions or coroutines).
 */
template<typename Handler>
class basic_router {
public:
  using handler_type = Handler;

  struct match {
    /// The matched handler, nullptr if no route matched
    const handler_type* handler = nullptr;

    /// Route parameters (capture groups)
    std::vector<std::string> params;

    /// When the path matched but not the method: methods allowed for this path, without duplicates (for 405 and
    /// Allow)
    std::vector<std::string> allowed_methods;

    explicit operator bool() const { return handler != nullptr; }
  };

  void add(std::string method, const std::string& pattern, handler_type handler)
  {
    routes_.push_back({std::move(method), pattern, std::regex(pattern), std::move(handler)});
  }

  /// Finds the handler for a request. HEAD falls back to GET when no HEAD route matches.
  match find(std::string_view method, std::string_view path) const
  {
    match m = find_exact(method, path);

    if (!m && method == "HEAD") {
      match get = find_exact("GET", path);

      if (get)
        return get;
    }

    return m;
  }

  bool empty() const { return routes_.empty(); }

private:
  struct route {
    std::string method;
    std::string pattern;
    std::regex regex;
    handler_type handler;
  };

  match find_exact(std::string_view method, std::string_view path) const
  {
    match m;
    std::match_results<std::string_view::const_iterator> groups;

    for (const auto& r : routes_) {
      if (!std::regex_match(path.begin(), path.end(), groups, r.regex))
        continue;

      if (r.method != method) {
        add_unique(m.allowed_methods, r.method);
        continue;
      }

      m.handler = &r.handler;
      m.allowed_methods.clear();

      for (std::size_t i = 1; i < groups.size(); ++i)
        m.params.push_back(groups[i].str());

      return m;
    }

    return m;
  }

  static void add_unique(std::vector<std::string>& values, const std::string& value)
  {
    if (std::find(values.begin(), values.end(), value) == values.end())
      values.push_back(value);
  }

private:
  std::vector<route> routes_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_SERVER_ROUTER_HPP
