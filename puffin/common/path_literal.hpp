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

#ifndef PFN_PATH_LITERAL_HPP
#define PFN_PATH_LITERAL_HPP

#include "string_literal.hpp"

template<size_t N>
constexpr bool is_int(const string_literal<N>& rhs, size_t pos) {
  return rhs.compare_n("{Int}", pos);
}

template<size_t N>
constexpr bool is_string(const string_literal<N>& rhs, size_t pos) {
  return rhs.compare_n("{String}", pos);
}

template<size_t N>
constexpr bool is_float(const string_literal<N>& rhs, size_t pos) {
  return rhs.compare_n("{Float}", pos);
}

template<size_t N>
constexpr bool is_bool(const string_literal<N>& rhs, size_t pos) {
  return rhs.compare_n("{Bool}", pos);
}

template<size_t N>
constexpr int tag_for_path_type(const string_literal<N>& rhs, size_t pos) {
  return is_int(rhs, pos) ?
           0x1 :
           is_string(rhs, pos) ?
             0x2 :
             is_float(rhs, pos) ?
              0x03 :
              is_bool(rhs, pos) ?
                0x04 :
                0;

}

template<size_t N>
constexpr uint64_t tag_for_path_r(const string_literal<N>& rhs, size_t pos, size_t r = 0) {
  return pos > 0 ?
           rhs[pos] == '{' ?
             tag_for_path_type(rhs, pos) << (r * 4) | tag_for_path_r(rhs, pos - 1, r + 1)
             : tag_for_path_r(rhs, pos-1, r)
           : 0;
}

template<size_t N>
constexpr uint64_t tag_for_path(const string_literal<N>& rhs) {
  return tag_for_path_r(rhs, N-1, 0);
}

template<size_t N>
void re_for_path(const string_literal<N>& rhs) {

}

#endif // PFN_PATH_LITERAL_HPP
