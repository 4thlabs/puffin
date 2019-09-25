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

#ifndef PFN_CALL_TRAITS_HPP
#define PFN_CALL_TRAITS_HPP

#include <tuple>

template<typename T>
struct call_argument { static const int value = 0x0; };

template<>
struct call_argument<int> { static const int value = 0x1; };

template<>
struct call_argument<std::string> { static const int value = 0x2; };

template<>
struct call_argument<float> { static const int value = 0x3; };

template<>
struct call_argument<bool> { static const int value = 0x4; };

template<typename... Args>
struct call_tag;

template<>
struct call_tag<> {
  static const int value = 0;
};

template<typename Arg, typename... Args>
struct call_tag<Arg, Args...> {
  static const uint64_t value = call_tag<Args...>::value | call_argument<Arg>::value << (4 * sizeof...(Args));
};

template<typename F>
struct call_traits : public call_traits<decltype(&F::operator())> {
};

template<typename Class, typename Return, typename... Args>
struct call_traits<Return(Class::*)(Args...) const> {
  using class_type  = Class;
  using result_type = Return;

  static constexpr size_t arity = sizeof...(Args);

  template<size_t i>
  struct arg {
    using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
  };

  static constexpr int values[] = { call_argument<Args>::value... };

  static constexpr uint64_t tag = call_tag<Args...>::value;
};

template<typename Class, typename Return, typename... Args>
constexpr int call_traits<Return(Class::*)(Args...) const>::values[];

template<typename F, typename Traits = call_traits<F>>
class call : public Traits {
public:
  using traits = Traits;

  call(F&& f)
    : f_(f)
  {}

  private:
    F f_;
};



#endif // PFN_CALL_TRAITS_HPP
