//  ____         __  __ _       
// |  _ \ _   _ / _|/ _(_)_ __  
// | |_) | | | | |_| |_| | '_  | 
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|          
//
// BSD 3-Clause License

// Copyright (c) 2019, Thomas Gourgues (thomas.gourgues@gmail.com)
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

#ifndef URI_PARSER_HPP
#define URI_PARSER_HPP

#include "uri_exceptions.hpp"

#include <regex>
#include <string>

namespace pfn {

template <typename P> class basic_uri;

struct regex_parser {
  const int group_scheme = 1;
  const int group_username = 2;
  const int group_password = 3;
  const int group_host = 4;
  const int group_port = 5;
  const int group_path = 6;
  const int group_fragment = 7;
  const int group_query_parameters = 8;

  template <typename P>
  void parse(basic_uri<P>& uri, const std::string& url) {
    std::regex re(R"(^)"
                  R"((?:(http[s]?|ftp)://)"   // Scheme
                  R"((?:(\S*):(\S*)@)?)"      // User : Pass @
                  R"(([\w\-\.]+))"          // Host
                  R"((?::([0-9]{2,5}))?)?)"  // Port
                  R"((/[\S][^#\?]+)?)"      // Path
                  R"(/?)"
                  R"((?:#([\S][^\?]+))?)"     // Fragment
                  R"(/?)"
                  R"((?:\?(([^&]*=[^&]*&?)*))?)" // Query parameters
                  R"($)");

    std::smatch matches;

    if (std::regex_match(url, matches, re)) {
     /* LOG(L_DEBUG) << matches.size();
      for (auto& m : matches) {
        LOG(L_DEBUG) << m.str();
      }
     */

      uri.scheme_ = matches[group_scheme].str();
      uri.host_ = matches[group_host].str();

      std::string port = matches[group_port].str();
      if (port.size() > 0) {
        uri.port_ = std::stoi(port);
      }

      uri.path_ = matches[group_path].str();
      uri.fragment_ = matches[group_fragment].str();

      // Extracting query parameters
      uri.query_string_ = matches[group_query_parameters].str();
    } else {
      throw new parsing_exception();
    }
  }
};

}

#endif // URI_PARSER_HPP
