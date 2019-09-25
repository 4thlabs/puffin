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

#include <puffin/common/path_literal.hpp>
#include <puffin/common/call_traits.hpp>

#include <iostream>

struct bs {

};

template<typename F, size_t N>
constexpr bool compare_compiletime(string_literal<N> r, F f) {

  return false;
}

template<typename F, size_t N>
bool compare_runtime(string_literal<N> r, F&& f) {

  return false;
}

int main(int argc, char** argv) {

   constexpr auto l = "/test/{Int}/tutu/{String}/{String}"_sl;
   constexpr auto l2 = "/test/{Int}/tutu"_sl;

   auto f = [](int i, std::string s, std::string s2) {
     return 0;
   };

   auto f2 = [](int i, std::string s, std::string s2, bs p) {
     return 0;
   };

   static_assert((call_traits<decltype(f)>::tag % tag_for_path(l)) == 0, "Route handler parameters are wrong");

   constexpr long i1 = 0xF;
   constexpr long i2 = 0xA;
   constexpr int i3 = i1 | (i2 << 4);

   call_traits<decltype(f)>::result_type t;

   //bool res = compare_compiletime(l2, f);

   for (int i = 0; i < call_traits<decltype(f)>::arity; i++) {
     std::cout << call_traits<decltype(f)>::values[i] << std::endl;
   }

   int index = 2;
   constexpr int i = l.find_first_of('{', 0);
   constexpr bool b = is_int(l, l.find_first_of('{', 0));

   static_assert(b, "is not string");

   bool equal = (l == "/test/{Int}/tutu");
   std::cout << (l == "/test/{Int}/tutu") << " " << call_argument<call_traits<decltype(f)>::arg<1>::type>::value << " " << l.find_first_of('{', 8) << std::endl;
   std::cout << "Tag from call: " << std::hex << call_traits<decltype(f)>::tag << std::endl;
   std::cout << "Tag from path: " << std::hex << tag_for_path(l) << std::endl;

   return 0;
}
