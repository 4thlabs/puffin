//  ____         __  __ _
// |  _ \ _   _ / _|/ _(_)_ __
// | |_) | | | | |_| |_| | '_  |
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|
//
// BSD 3-Clause License

// Copyright (c) 2025, Thomas Gourgues (thomas.gourgues@gmail.com)
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

#ifndef PUFFIN_WEBKIT_TRANSPORT_CONCEPTS_HPP
#define PUFFIN_WEBKIT_TRANSPORT_CONCEPTS_HPP

#include <puffin/webkit/detail/awaitable.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace puffin {
namespace webkit {

/**
 * @brief A connected byte stream (TCP socket, TLS stream, in-memory pipe...).
 *
 *  - read_some(buffer) reads at least one byte into buffer, co_await returns the number of bytes read, 0 at end
 *    of stream,
 *  - write(data) writes all of data, co_await returns the number of bytes written,
 *  - close() closes the stream, pending operations complete with an error.
 *
 * Errors are reported by throwing from co_await. Adapters (asio, Qt...) implement this concept, the HTTP server
 * and client only rely on it.
 */
template<typename S>
concept Stream = requires(S& s, std::span<char> in, std::span<const char> out) {
  { s.read_some(in) } -> AwaitableOf<std::size_t>;
  { s.write(out) } -> AwaitableOf<std::size_t>;
  s.close();
};

/**
 * @brief Accepts incoming connections, co_await on accept() returns a connected Stream
 */
template<typename A>
concept Acceptor = requires(A& a) {
  { a.accept() } -> Awaitable;
  requires Stream<await_result_t<decltype(a.accept())>>;
  a.close();
};

/**
 * @brief Opens outgoing connections, co_await on connect(host, port) returns a connected Stream.
 *
 * Name resolution, and for secured connectors the TLS handshake, are done by connect().
 */
template<typename C>
concept Connector = requires(C& c, std::string_view host, std::uint16_t port) {
  { c.connect(host, port) } -> Awaitable;
  requires Stream<await_result_t<decltype(c.connect(host, port))>>;
};

/// The stream type produced by an acceptor or a connector
template<Acceptor A>
using accepted_stream_t = await_result_t<decltype(std::declval<A&>().accept())>;

template<Connector C>
using connected_stream_t =
    await_result_t<decltype(std::declval<C&>().connect(std::string_view(), std::uint16_t()))>;

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_TRANSPORT_CONCEPTS_HPP
