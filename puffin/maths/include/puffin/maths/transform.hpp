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
//   and/or other m1erials provided with the distribution.

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

#ifndef PFN_MATHS_TRANSFORM
#define PFN_MATHS_TRANSFORM

#include <puffin/maths/matrix4.hpp>
#include <puffin/maths/vector3.hpp>

namespace pfn {

///
/// Fov Y Perspective
///
template<typename T>
basic_matrix4<T> perspective(T yfov, T aspect, T znear, T zfar)
{
  return basic_matrix4<T> {
    1.0 / aspect * std::tan(0.5 * yfov), 0.0, 0.0, 0.0,
    0.0, 1.0 / std::tan(0.5 * yfov), 0.0, 0.0,
    0.0, 0.0, (zfar + znear) / (znear - zfar), (2 * zfar * znear) / (znear - zfar),
    0.0, 0.0, -1.0, 0.0
  };
}

template<typename T>
basic_matrix4<T> translate(const basic_matrix4<T>& m, const basic_vector3<T>& v)
{
  auto res = m * basic_matrix4<T> {
    1.0, 0.0, 0.0, v.x,
    0.0, 1.0, 0.0, v.y,
    0.0, 0.0, 1.0, v.z,
    0.0, 0.0, 0.0, 1.0
  };

  return res;
}

template<typename T>
basic_matrix4<T> rotate(const basic_matrix4<T>& m, const basic_vector3<T>& v)
{
  auto res = m * basic_matrix4<T> {
    1.0, 0.0, 0.0, v.x,
    0.0, 1.0, 0.0, v.y,
    0.0, 0.0, 1.0, v.z,
    0.0, 0.0, 0.0, 1.0
  };

  return res;
}

} // namespace pfn


#endif // PFN_MATHS_TRANSFORM