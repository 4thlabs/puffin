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

#ifndef PUFFIN_WEBKIT_URI_PARSER_HPP
#define PUFFIN_WEBKIT_URI_PARSER_HPP

#include <puffin/webkit/uri/uri_exceptions.hpp>

#include <cstdint>
#include <regex>
#include <string>

namespace puffin {
namespace webkit {

template<typename P>
class basic_uri;

/**
 * @brief Default uri parsing policy, based on a regular expression
 */
struct regex_parser {
  static constexpr int group_scheme = 1;
  static constexpr int group_username = 2;
  static constexpr int group_password = 3;
  static constexpr int group_host = 4;
  static constexpr int group_port = 5;
  static constexpr int group_path = 6;
  static constexpr int group_fragment = 7;
  static constexpr int group_query_parameters = 8;

  template<typename P>
  void parse(basic_uri<P>& uri, const std::string& url) const
  {
    static const std::regex re(R"(^)"
                               R"((?:(http[s]?|ftp)://)"         // Scheme
                               R"((?:(\S*):(\S*)@)?)"            // User : Pass @
                               R"(([\w\-\.]+))"                  // Host
                               R"((?::([0-9]{2,5}))?)?)"         // Port
                               R"((/[^#\?\s]*)?)"                // Path
                               R"(/?)"
                               R"((?:#([^\?\s]+))?)"             // Fragment
                               R"(/?)"
                               R"((?:\?(([^&]*=[^&]*&?)*))?)"    // Query parameters
                               R"($)");

    std::smatch matches;

    if (!std::regex_match(url, matches, re))
      throw uri_parsing_error(url);

    uri.scheme_ = matches[group_scheme].str();
    uri.host_ = matches[group_host].str();

    std::string port = matches[group_port].str();
    if (!port.empty())
      uri.port_ = static_cast<std::uint16_t>(std::stoul(port));

    uri.path_ = matches[group_path].str();
    uri.fragment_ = matches[group_fragment].str();
    uri.query_string_ = matches[group_query_parameters].str();
  }
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_URI_PARSER_HPP
