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

#include <catch2/catch.hpp>
#include "puffin/ioc/ioc.hpp"

template<typename... Args>
class cont : public pfn::ioc::container<Args...> {
public:

};

class obj1 {
public:
  int value = 2;
};

class obj2 {
public:
  obj2(std::shared_ptr<obj1> c)
    : o1(c)
  {}

  std::shared_ptr<obj1> o1;
};

class obj3 {
public:
  obj3(std::shared_ptr<obj1> o1, std::shared_ptr<obj2> o2) 
  {

  }
};

template<>
struct pfn::ioc::dependencies<obj2> : pfn::ioc::bind<obj1> {};

template<>
struct pfn::ioc::dependencies<obj3> : pfn::ioc::bind<obj1, obj2> {};

cont<obj1, obj2, obj3> c;

TEST_CASE("ioc", "[ioc]") {
  SECTION("initialization") {
    auto l = c.get<obj1>();
    REQUIRE(l->value == 2);
    l->value = 3;

    auto l2 = c.get<obj2>();
    REQUIRE(l2->o1->value == 3);
  }
}