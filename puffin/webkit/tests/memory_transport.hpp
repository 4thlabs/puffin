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

#ifndef PUFFIN_WEBKIT_TESTS_MEMORY_TRANSPORT_HPP
#define PUFFIN_WEBKIT_TESTS_MEMORY_TRANSPORT_HPP

#include <algorithm>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {
namespace test {

/// An awaiter that is always ready
template<typename T>
struct ready {
  T value;

  bool await_ready() const noexcept { return true; }
  void await_suspend(std::coroutine_handle<>) const noexcept {}
  T await_resume() { return std::move(value); }
};

/// Both ends of an in-memory connection, shared by the stream and the test
struct pipe {
  std::string input;  ///< Bytes the stream will read
  std::string output; ///< Bytes written to the stream
  std::size_t max_read = 7; ///< Small reads, to exercise incremental parsing
  bool closed = false;
};

/// A Stream reading and writing a pipe, completing synchronously
struct memory_stream {
  std::shared_ptr<pipe> p = std::make_shared<pipe>();

  ready<std::size_t> read_some(std::span<char> buffer)
  {
    std::size_t n = p->closed ? 0 : std::min({buffer.size(), p->input.size(), p->max_read});
    std::memcpy(buffer.data(), p->input.data(), n);
    p->input.erase(0, n);
    return {n};
  }

  ready<std::size_t> write(std::span<const char> data)
  {
    p->output.append(data.data(), data.size());
    return {data.size()};
  }

  void close() { p->closed = true; }
};

/// A Connector handing out pipes prepared by the test, in order
struct memory_connector {
  std::shared_ptr<std::deque<std::shared_ptr<pipe>>> pipes = std::make_shared<std::deque<std::shared_ptr<pipe>>>();
  std::shared_ptr<std::size_t> connections = std::make_shared<std::size_t>(0);

  std::shared_ptr<pipe> add(std::string input)
  {
    auto p = std::make_shared<pipe>();
    p->input = std::move(input);
    pipes->push_back(p);
    return p;
  }

  ready<memory_stream> connect(std::string_view, std::uint16_t)
  {
    ++*connections;

    if (pipes->empty())
      throw std::runtime_error("connection refused");

    memory_stream s{pipes->front()};
    pipes->pop_front();
    return {std::move(s)};
  }
};

} // namespace test
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_TESTS_MEMORY_TRANSPORT_HPP
