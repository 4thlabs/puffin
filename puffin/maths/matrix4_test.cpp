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
#include <iostream>
#include <puffin/maths/matrix4.hpp>

pfn::matrix4f identity {
  1.0, 0.0, 0.0, 0.0,
  0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0
};

pfn::matrix4f t1 {
  5.0, 3.2, -4.1, 6.9,
  2.0, -4.0, 3.0, 2.3,
  7.0, 3.0, -3.4, 2.0,
  -8.0, 2.0, 5.2, 4.1
};

pfn::matrix4f t2 {
  3.0, 2.6, -7.1, -3.9,
  -6.0, 2.1, 3.2, 8.0,
  5.0, 4.7, 7.2, -6.0,
  3.0, 7.0, 3.5, 6.0
};

pfn::matrix4f t3 {
  -4.0, 48.75, -30.63, 72.1,
  51.9, 27.0, 2.65, -44.0,
  -8.0, 22.52, -57.58, 29.1,
  2.3, 36.54, 114.99, 40.6
};

TEST_CASE("matrix operations", "[maths][matrix]") {
  SECTION("equality") {
    pfn::matrix4f m;

    REQUIRE(m == identity);
  }

  SECTION("multiplication") {
    pfn::matrix4f m = t1 * t2;
    std::cout << m << std::endl;
    REQUIRE(m == t3);
  }

  SECTION("addition") {
    
  }
}
