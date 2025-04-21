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

#ifndef PFN_INTEGER_SEQUENCE_HPP
#define PFN_INTEGER_SEQUENCE_HPP

#include <cstddef>

///
/// Integer and index sequence based on
/// http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence/17426611#17426611
///
template<typename T, T... Ints>
struct integer_sequence {
  using type = integer_sequence<T, Ints...>;
  using value_type = T;
  static constexpr std::size_t size() { return sizeof...(Ints); }
  static constexpr T values[] = { Ints... };
};

template<typename T, typename S1, typename S2>
struct merge_sequences {};

template<typename T, T... I1, T... I2>
struct merge_sequences<T, integer_sequence<T, I1...>, integer_sequence<T, I2...>>
  : integer_sequence<T, I1..., (sizeof...(I1)+I2)...>
{};

template<typename T, T Nt, std::size_t N = Nt>
struct make_integer_sequence
  : merge_sequences
    <
      T,
      typename make_integer_sequence<T, N/2>::type,
      typename make_integer_sequence<T, N - N/2>::type
    >
{};

template<typename T, T... Ints>
constexpr T integer_sequence<T, Ints...>::values[];

template<typename T, T N>
struct make_integer_sequence<T, N, 0> : integer_sequence<T> {} ;

template<typename T, T N>
struct make_integer_sequence<T, N, 1> : integer_sequence<T, T(0)> {} ;

// Types and helpers for index sequence

template<std::size_t... Ints>
using index_sequence = integer_sequence<std::size_t, Ints...>;

template<size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

template<typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

#endif // PFN_INTEGER_SEQUENCE_HPP
