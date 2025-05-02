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

#ifndef PFN_WEBKITHEADER_HPP
#define PFN_WEBKITHEADER_HPP

#include <string>
#include <ostream>

namespace pfn {

/// Default header class
template<typename CharT>
class basic_header {
public:
  using String = std::basic_string<CharT>;

  basic_header() {}

  basic_header(const String& name, const String& value)
    : name_(name), value_(value)
  {}

  explicit basic_header(const String& name, int value)
    : name_(name), value_(std::to_string(value))
  {}

  virtual ~basic_header() {}

  const String& name() const {
    return name_;
  }

  const String& value() const {
    return value_;
  }

  template<typename T>
  friend std::ostream& operator<<(std::ostream& o, const basic_header<T>& header);

private:
  String name_;
  String value_;
};

template<typename CharT>
inline std::ostream& operator<<(std::ostream& o, const basic_header<CharT>& header) {
  if (header.value_.size() > 0) {
    o << header.name_ << ": " << header.value_ << "\r\n";
  }
  
  return o;
}

using header = basic_header<char>;

}

#endif // PFN_WEBKIT_HEADER_H
