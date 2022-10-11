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

#ifndef PFN_MATHS_VECTOR3
#define PFN_MATHS_VECTOR3

#include <iostream>
#include <iomanip>
#include "precision.hpp"

namespace pfn {

template<typename T>
class basic_vector3 {
public:
  inline basic_vector3()
  {
      x = y = z = 0;
  }

  inline basic_vector3(T fx, T fy, T fz)
  {
      x = static_cast<T>(fx); y = static_cast<T>(fy); z = static_cast<T>(fz);
  }

  inline basic_vector3<T>& operator= (const basic_vector3<T>& v)
  {
      x = v.x;
      y = v.y;
      z = v.z;

      return *this;
  }

  inline basic_vector3<T>& operator+= (const basic_vector3<T>& v)
  {
      x += v.x;
      y += v.y;
      z += v.z;

      return *this;
  }

  inline basic_vector3<T>& operator-= (const basic_vector3<T>& v)
  {
      x -= v.x;
      y -= v.y;
      z -= v.z;

      return *this;
  }

  inline basic_vector3<T>& operator*= (const T f)
  {
      x *= f;
      y *= f;
      z *= f;

      return *this;
  }

  inline basic_vector3<T>& operator/= (const T f)
  {
      x /= f;
      y /= f;
      z /= f;

      return *this;
  }

  inline basic_vector3<T> operator+ (const basic_vector3<T>& v) const
  {
      return basic_vector3<T>(x+v.x, y+v.y, z+v.z);
  }

  inline basic_vector3<T> operator- (const basic_vector3<T>& v) const
  {
      return basic_vector3(x-v.x, y-v.y, z-v.z);
  }

  inline basic_vector3<T> operator* (const basic_vector3<T>& v) const
  {
      return basic_vector3(x*static_cast<T>(v.x), y*static_cast<T>(v.y), z*static_cast<T>(v.z));
  }

  inline basic_vector3<T> operator* (const T f) const
  {
      return basic_vector3<T>(x*f, y*f, z*f);
  }

  inline basic_vector3<T> operator/ (const T f) const
  {
      return basic_vector3<T>(x/f, y/f, z/f);
  }

  inline basic_vector3<T> operator+ () const
  {
      return basic_vector3<T>(x, y, z);
  }

  inline basic_vector3<T> operator- () const
  {
      return basic_vector3<T>(-x, -y, -z);
  }

  inline bool operator== (const basic_vector3<T>& rhs) const
  {
    return equals(x, rhs.x) && equals(y, rhs.y) && equals(z, rhs.z);
  } 

public:
  union {
    struct {
      T x, y, z;
    };
    T v[3];
  };
};

template<typename T>
inline std::ostream& operator<< (std::ostream& os, const basic_vector3<T>& v)
{
  os << std::setprecision(10) << "vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
  return os; 
}

using vector3f = basic_vector3<float>;

}

#endif // PFN_MATHS_VECTOR3