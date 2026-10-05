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

#ifndef PFN_STRING_LITERAL_HPP
#define PFN_STRING_LITERAL_HPP

#include <string>
#include <iostream>

///
/// Staticaly compiled strings
///
template<typename CharT, size_t N, typename Traits = std::char_traits<CharT>>
class basic_string_literal {
public:
  static constexpr size_t type_size = N;
  using traits_type = Traits;

  constexpr basic_string_literal()
    : str_{}
  {}

  constexpr basic_string_literal(const char (&S)[N])
    : str_(S)
  {}

  template<typename... C>
  constexpr basic_string_literal(C... cs)
    : str_{cs..., '\0'}
  {}

  constexpr size_t size() const {
    return type_size;
  }

  template<size_t I>
  constexpr char char_at() const {
    static_assert(I < N, "String literal overflow");
    return str_[I];
  }

  template<size_t M>
  constexpr bool compare(const char (&c)[M], size_t pos) const {
    return N == M-1 ? ((pos < N) && (pos < M) ? (str_[pos] == c[pos] ? compare(c, pos + 1) : false) : true) : false;
  }

  template<size_t M>
  constexpr bool compare(const char * c, size_t pos) const {
    return N == M ? ((pos < N) && (pos < M) ? (str_[pos] == c[pos] ? compare<M>(c, pos + 1) : false) : true) : false;
  }

  template<size_t M>
  constexpr bool compare(const basic_string_literal<CharT, M, Traits>& rhs, size_t pos) const {
    return compare<M>(rhs.c_str(), pos);
  }

  template<size_t M>
  constexpr bool compare_n(const char (&c)[M], size_t pos, size_t i = 0) const {
    return (pos + i < N) && (i < M-1) ? (str_[pos+i] == c[i] ? compare_n(c, pos, i + 1) : false) : true;
  }

  /*template<size_t M>
  constexpr bool compare_n(const basic_string_literal<CharT, M, Traits>& rhs, size_t pos, size_t n) const {
    return compare<M>(rhs.c_str(), pos);
  }*/

  constexpr int find_first_of(const char c, size_t pos) const {
    return pos < N ? (str_[pos] == c ? pos : find_first_of(c, pos + 1)) : -1;
  }

  constexpr char operator[](size_t index) const {
    return str_[index];
  }

  constexpr const char* c_str() const {
    return &str_[0];
  }

private:
  CharT str_[N+1];
};

template<typename CharT, typename Traits, size_t N, size_t M>
constexpr bool operator== (const basic_string_literal<CharT, N, Traits>& lhs, const char (&rhs)[M]) {
  return lhs.compare(rhs, 0);
}

template<typename CharT, typename Traits, size_t N, size_t M>
constexpr bool operator== (const basic_string_literal<CharT, N, Traits>& lhs, const basic_string_literal<CharT, M, Traits>& rhs) {
  return lhs.compare(rhs, 0);
}

template<size_t I, size_t N, typename CharT, typename Traits = std::char_traits<CharT>>
constexpr char char_at(const basic_string_literal<CharT, N, Traits>& sl) {
  static_assert(I < basic_string_literal<CharT, N, Traits>::type_size, "String literal overflow");
  return sl[I];
}

template<size_t N>
using string_literal = basic_string_literal<char, N>;

template<size_t N>
constexpr auto make_string_literal(const char (&S)[N]) -> string_literal<N> {
  return string_literal<N>(S);
}

template<typename CharT, CharT... cs>
constexpr string_literal<sizeof...(cs)> operator"" _sl () {
  return string_literal<sizeof...(cs)>(cs...);
}


//using wstring_literal = basic_string_literal<wchar_t>;
//using utf8_string_literal = basic_string_literal<char16_t>;
//using utf16_wstring_literal = basic_string_literal<char32_t>;

#endif // PFN_STRING_LITERAL_HPP
