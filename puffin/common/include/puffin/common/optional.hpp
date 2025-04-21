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

#ifndef PFN_OPTIONAL_HPP
#define PFN_OPTIONAL_HPP

#include <puffin/common/none.hpp>
#include <string>

namespace pfn {

///
/// Optional type
///
template<typename T>
class optional {
public:
  using value_type = T;

  optional(none_t n)
    : initialized_(false)
  {}

  /// Default copy constructor
  optional(const optional<T>& o)
    : initialized_(o.initialized_), value_(o.value_)
  {}

  /// Default move constructor
  optional(optional<T>&& o)
    : initialized_(o.initialized_), value_(std::move(o.value_)) {
    o.initialized_ = false;
  }

  /// Copy constructor with T
  optional(const T& t)
    : initialized_(true), value_(t)
  {}

  /// Move constructor with T
  optional(T&& t)
    : initialized_(true), value_(std::move(t))
  {}

  virtual ~optional() {}

  operator bool() const { return initialized_; }

  const value_type& get() const { return value_; }
  value_type& get() { return value_; }

private:
  bool         initialized_;
  value_type   value_;
};

/// Default method for creating an optionale value
template<typename T>
auto make_optional(bool condition, const T& t) -> optional<T> {
  return condition ? optional<T>(t) : optional<T>(none);
}

}

#endif // PFN_OPTIONAL_HPP
