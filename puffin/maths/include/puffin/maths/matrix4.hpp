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

#ifndef PFN_MATHS_MATRIX4
#define PFN_MATHS_MATRIX4

#include <ostream>
#include <cstring>
#include "precision.hpp"

namespace pfn {

template<typename T>
class basic_matrix4 {
public:
  inline basic_matrix4()
  {
    identity();
  }

  inline basic_matrix4(const basic_matrix4<T>& m1)
  {
    memcpy(&_11, &m1, sizeof(basic_matrix4<T>));
  }
  
  inline basic_matrix4(T t11, T t12, T t13, T t14,
                       T t21, T t22, T t23, T t24,
                       T t31, T t32, T t33, T t34,
                       T t41, T t42, T t43, T t44)
  {
    _11 = t11; _12 = t12; _13 = t13; _14 = t14;
    _21 = t21; _22 = t22; _23 = t23; _24 = t24;
    _31 = t31; _32 = t32; _33 = t33; _34 = t34;
    _41 = t41; _42 = t42; _43 = t43; _44 = t44;
  }

  inline T& operator() (unsigned int col, unsigned int row)
  {
    return m[col][row];
  }

  inline T operator() (unsigned int col, unsigned int row) const
  {
    return m[col][row];
  }

  inline operator T* ()
  {
    return static_cast<T*>(&_11);
  }

  inline operator const T* () const
  {
    return static_cast<const T*>(&_11);
  }

  inline basic_matrix4& operator= (const basic_matrix4<T>& mat)
  {
    memcpy(&_11, &mat, sizeof(basic_matrix4<T>));

    return *this;
  }

  inline basic_matrix4<T>& operator*= (const basic_matrix4<T>& mat)
  {
    (*this) = multiply(*this, mat);

    return *this;
  }

  inline basic_matrix4<T>& operator+= (const basic_matrix4<T>& mat)
  {
    _11 += mat._11; _12 += mat._12; _13 += mat._13; _14 += mat._14;
    _21 += mat._21; _22 += mat._22; _23 += mat._23; _24 += mat._24;
    _31 += mat._31; _32 += mat._32; _33 += mat._33; _34 += mat._34;
    _41 += mat._41; _42 += mat._42; _43 += mat._43; _44 += mat._44;

    return *this;
  }

  inline basic_matrix4<T>& operator-= (const basic_matrix4<T>& mat)
  {
    _11 -= mat._11; _12 -= mat._12; _13 -= mat._13; _14 -= mat._14;
    _21 -= mat._21; _22 -= mat._22; _23 -= mat._23; _24 -= mat._24;
    _31 -= mat._31; _32 -= mat._32; _33 -= mat._33; _34 -= mat._34;
    _41 -= mat._41; _42 -= mat._42; _43 -= mat._43; _44 -= mat._44;

    return *this;
  }

  inline basic_matrix4<T>& operator*= (const T f)
  {
    _11 *= f; _12 *= f; _13 *= f; _14 *= f;
    _21 *= f; _22 *= f; _23 *= f; _24 *= f;
    _31 *= f; _32 *= f; _33 *= f; _34 *= f;
    _41 *= f; _42 *= f; _43 *= f; _44 *= f;

    return *this;
  }

  inline basic_matrix4<T>& operator/= (const float f)
  {
    T inv = 1.0f / f;
    _11 *= inv; _12 *= inv; _13 *= inv; _14 *= inv;
    _21 *= inv; _22 *= inv; _23 *= inv; _24 *= inv;
    _31 *= inv; _32 *= inv; _33 *= inv; _34 *= inv;
    _41 *= inv; _42 *= inv; _43 *= inv; _44 *= inv;

    return *this;
  }

  inline basic_matrix4<T> operator* (const basic_matrix4<T>& m) const
  {
    return multiply(*this, m);
  }

  inline basic_matrix4<T> operator+ (const basic_matrix4<T>& m1) const
  {
    return basic_matrix4<T>(_11 + m1._11, _12 + m1._12, _13 + m1._13, _14 + m1._14,
                            _21 + m1._21, _22 + m1._22, _23 + m1._23, _24 + m1._24,
                            _31 + m1._31, _32 + m1._32, _33 + m1._33, _34 + m1._34,
                            _41 + m1._41, _42 + m1._42, _43 + m1._43, _44 + m1._44);
  }

  inline basic_matrix4<T> operator- (const basic_matrix4<T>& m1) const
  {
    return basic_matrix4<T>(_11 - m1._11, _12 - m1._12, _13 - m1._13, _14 - m1._14,
                            _21 - m1._21, _22 - m1._22, _23 - m1._23, _24 - m1._24,
                            _31 - m1._31, _32 - m1._32, _33 - m1._33, _34 - m1._34,
                            _41 - m1._41, _42 - m1._42, _43 - m1._43, _44 - m1._44);
  }

  inline basic_matrix4<T> operator* (T f) const
  {
    return basic_matrix4<T>(_11 * f, _12 * f, _13 * f, _14 * f,
                            _21 * f, _22 * f, _23 * f, _24 * f,
                            _31 * f, _32 * f, _33 * f, _34 * f,
                            _41 * f, _42 * f, _43 * f, _44 * f);
  }

  inline bool operator== (const basic_matrix4<T>& m) const
  {
    return equals(_11, m._11) && equals(_12, m._12) && equals(_13, m._13) && equals(_14, m._14) &&
           equals(_21, m._21) && equals(_22, m._22) && equals(_23, m._23) && equals(_24, m._24) &&
           equals(_31, m._31) && equals(_32, m._32) && equals(_33, m._33) && equals(_34, m._34) &&
           equals(_41, m._41) && equals(_42, m._42) && equals(_43, m._43) && equals(_44, m._44);
  }

  inline bool operator!= (const basic_matrix4<T>& mat) const
  {
    return (memcmp(this, &mat, sizeof(basic_matrix4<T>)) != 0);
  }


  inline basic_matrix4<T> identity()
  {
    _11 = 1.0; _12 = 0.0; _13 = 0.0; _14 = 0.0;
    _21 = 0.0; _22 = 1.0; _23 = 0.0; _24 = 0.0;
    _31 = 0.0; _32 = 0.0; _33 = 1.0; _34 = 0.0;
    _41 = 0.0; _42 = 0.0; _43 = 0.0; _44 = 1.0;

    return (*this);
  }

public:
  union {
    struct { //Visual representation _ij where i is line number and j column number
      T _11, _21, _31, _41; // Col 1
      T _12, _22, _32, _42; // Col 2
      T _13, _23, _33, _43; // Col 3
      T _14, _24, _34, _44; // Col 4
    };
    T m[4][4];
  };
};

template<typename T>
inline basic_matrix4<T> multiply(const basic_matrix4<T>& m1, const basic_matrix4<T>& m2)
{
  basic_matrix4<T> mat;

  mat._11 = m1._11 * m2._11 + m1._12 * m2._21 + m1._13 * m2._31 + m1._14 * m2._41;
  mat._12 = m1._11 * m2._12 + m1._12 * m2._22 + m1._13 * m2._32 + m1._14 * m2._42;
  mat._13 = m1._11 * m2._13 + m1._12 * m2._23 + m1._13 * m2._33 + m1._14 * m2._43;
  mat._14 = m1._11 * m2._14 + m1._12 * m2._24 + m1._13 * m2._34 + m1._14 * m2._44;

  mat._21 = m1._21 * m2._11 + m1._22 * m2._21 + m1._23 * m2._31 + m1._24 * m2._41;
  mat._22 = m1._21 * m2._12 + m1._22 * m2._22 + m1._23 * m2._32 + m1._24 * m2._42;
  mat._23 = m1._21 * m2._13 + m1._22 * m2._23 + m1._23 * m2._33 + m1._24 * m2._43;
  mat._24 = m1._21 * m2._14 + m1._22 * m2._24 + m1._23 * m2._34 + m1._24 * m2._44;

  mat._31 = m1._31 * m2._11 + m1._32 * m2._21 + m1._33 * m2._31 + m1._34 * m2._41;
  mat._32 = m1._31 * m2._12 + m1._32 * m2._22 + m1._33 * m2._32 + m1._34 * m2._42;
  mat._33 = m1._31 * m2._13 + m1._32 * m2._23 + m1._33 * m2._33 + m1._34 * m2._43;
  mat._34 = m1._31 * m2._14 + m1._32 * m2._24 + m1._33 * m2._34 + m1._34 * m2._44;

  mat._41 = m1._41 * m2._11 + m1._42 * m2._21 + m1._43 * m2._31 + m1._44 * m2._41;
  mat._42 = m1._41 * m2._12 + m1._42 * m2._22 + m1._43 * m2._32 + m1._44 * m2._42;
  mat._43 = m1._41 * m2._13 + m1._42 * m2._23 + m1._43 * m2._33 + m1._44 * m2._43;
  mat._44 = m1._41 * m2._14 + m1._42 * m2._24 + m1._43 * m2._34 + m1._44 * m2._44;

  return mat;
}

template<typename T>
inline std::ostream& operator<< (std::ostream& os, const basic_matrix4<T>& mat)
{
    os << "Matrix4 {" << std::endl;
    os << " " << mat._11 << ", " << mat._12 << ", " << mat._13 << ", " << mat._14 << ", " << std::endl;
    os << " " << mat._21 << ", " << mat._22 << ", " << mat._23 << ", " << mat._24 << ", " << std::endl;
    os << " " << mat._31 << ", " << mat._32 << ", " << mat._33 << ", " << mat._34 << ", " << std::endl;
    os << " " << mat._41 << ", " << mat._42 << ", " << mat._43 << ", " << mat._44 << "  " << std::endl;
    os << "};" << std::endl;

    return os;
}

using matrix4f = basic_matrix4<float>;

}

#endif // PFN_MATHS_MATRIX4