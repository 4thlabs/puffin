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

#ifndef PFN_IOC_HPP
#define PFN_IOC_HPP

#include <tuple>
#include <memory>

#include <puffin/common/none.hpp>
#include <puffin/ioc/bind.hpp>

namespace pfn {
namespace ioc {

///
/// A really simple static ioc container
///
template<template<typename> typename Allocator = shared_ptr_allocator, typename... Objects>
class basic_container {
public:
  template<typename C>
  using contained_type = typename Allocator<C>::type;

  using contained_tuple_type = std::tuple<std::shared_ptr<Objects>...>;

public:
  basic_container() {
    auto l = {none, set(Allocator<Objects>::allocate(
      resolve<typename dependencies<Objects>::type>(
        std::make_index_sequence<dependencies<Objects>::arity>()
      ), std::make_index_sequence<dependencies<Objects>::arity>())
    )...};
  }

  template<typename C>
  contained_type<C> get()
  {
    return std::get<contained_type<C>>(objects_);
  }

private:
  template<typename C>
  none_t set(C rhs)
  {
    std::get<C>(objects_) = rhs;
    return none;
  }

  template<typename Tuple, size_t... I, 
           typename std::enable_if<std::is_same<Tuple, none_t>::value, int>::type = 0>
  std::tuple<none_t> resolve(std::index_sequence<I...>)
  {
    return std::make_tuple(none);
  }

  template<typename Tuple, size_t... I, 
           typename std::enable_if<!std::is_same<Tuple, none_t>::value, int>::type = 0>
  Tuple resolve(std::index_sequence<I...>)
  {
    return std::make_tuple(std::get<std::tuple_element_t<I, Tuple>>(objects_)...);
  }

private:
  contained_tuple_type objects_;
};

template<typename... Args>
using container = basic_container<shared_ptr_allocator, Args...>;

}
}

#endif // PFN_IOC_HPP