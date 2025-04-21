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

#ifndef PFN_IOC_ALLOCATOR_HPP
#define PFN_IOC_ALLOCATOR_HPP

namespace pfn {
namespace ioc {

///
/// A simple shared pointer allocator for the ioc container
///
template<typename C>
struct shared_ptr_allocator {
  using type = std::shared_ptr<C>;

  ///
  /// Allocates object with the corresponding dependendies
  ///
  template<typename Tuple, size_t... I,
           typename std::enable_if<!std::is_same<Tuple, std::tuple<none_t>>::value, int>::type = 0>
  static type allocate(Tuple&& t, std::index_sequence<I...>)
  {
    return std::make_shared<C>(std::get<I>(std::forward<Tuple>(t))...);
  }

  ///
  /// Allocates objects without dependencies
  ///
  template<typename Tuple, size_t... I,
           typename std::enable_if<std::is_same<Tuple, std::tuple<none_t>>::value, int>::type = 0>
  static type allocate(Tuple&& t, std::index_sequence<I...>)
  {
    return std::make_shared<C>();
  }
};

///
/// Fake allocator helper
///
template<typename C> struct none_allocator {};

}
}

#endif // PFN_IOC_ALLOCATOR_HPP