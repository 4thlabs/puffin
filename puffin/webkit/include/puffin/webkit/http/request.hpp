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

#ifndef PFN_WEBKIT_REQUEST_HPP
#define PFN_WEBKIT_REQUEST_HPP

#include "puffin/webkit/helpers/macros.hpp"
#include "puffin/webkit/http/header.hpp"
#include "puffin/webkit/http/cookie.hpp"
#include "puffin/webkit/http/status_code.hpp"
#include "puffin/webkit/http/session/session_base.hpp"
#include "puffin/webkit/http/session/session_traits.hpp"
#include "puffin/webkit/uri/uri.hpp"

#include <vector>
#include <unordered_map>
#include <string>
#include <ostream>


namespace pfn {

template<typename Tag>
class basic_request : public optional_session_base<Tag, basic_request<Tag>> {
public:
  using header_iterator = std::unordered_map<std::string, header>::iterator;
  using header_map = std::unordered_map<std::string, header>;
  using cookie_map = std::unordered_map<std::string, cookie>;

  basic_request() {}

  basic_request(const std::string& method, const uri& uri)
    : method_(method), uri_(uri)
  {}

  virtual ~basic_request() {}

  void add_header(const std::string& name, const std::string& value) {
    headers_.insert(std::make_pair(name, header(name, value)));
  }

  const header_iterator find_header(const std::string& name) {
    return headers_.find(name);
  }

  const header_iterator headers_end() {
    return headers_.end();
  }

  const header_map& headers() const {
    return headers_;
  }

  void add_cookie(cookie&& c) {
    cookies_.insert(std::make_pair(c.name(), std::move(c)));
  }

  const cookie_map& cookies() const {
    return cookies_;
  }

  void method(const std::string& method) {
    method_ = method;
  }

  const std::string& method() const {
    return method_;
  }

  void path(const std::string& path) {
    path_ = path;
  }

  const std::string& path() const {
    return path_;
  }

  const uri& uri() const {
    return uri_;
  }

  void body(const std::string& body) {
    body_ = body;
    headers_["Content-Length"] = header("Content-Length", std::to_string(body_.size()));
  }

  void add_parameter(const std::string& param) {
    parameters_.push_back(param);
  }

  const std::vector<std::string>& parameters() const {
    return parameters_;
  }

  template<typename T>
  friend std::ostream& operator<<(std::ostream& o, const basic_request<T>& request);

private:
  header_map headers_;
  cookie_map cookies_;

  std::string         method_;
  std::string         path_;
  std::string         body_;

  restpp::uri         uri_;

  std::vector<std::string> parameters_; // Path parameters
};

template<typename Tag>
inline std::ostream& operator<<(std::ostream& o, const basic_request<Tag>& request) {
  o << request.method_ << " " << request.uri_.path() << " HTTP/1.1\r\n";
  o << "Host: " << request.uri_.host() << "\r\n";

  for (auto& h : request.headers_) {
    o << h.second;
  }

  o << "\r\n";

  return o;
}

template<typename Tag>
struct is_request : std::false_type {};

template<typename Tag>
struct is_request<basic_request<Tag>> : std::true_type {};

RESTPP_EXTERN template class basic_request<tag::server>;
RESTPP_EXTERN template class basic_request<tag::client>;

using request = basic_request<tag::server>;

namespace client {
  using request = basic_request<tag::client>;
}


}



#endif // PFN_WEBKIT_REQUEST_HPP
