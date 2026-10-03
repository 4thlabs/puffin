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

#include <puffin/webkit/transport/concepts.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <coroutine>
#include <cstring>
#include <string>

using namespace puffin::webkit;

namespace {

/// An awaiter that is always ready
template<typename T>
struct ready {
  T value;

  bool await_ready() const noexcept { return true; }
  void await_suspend(std::coroutine_handle<>) const noexcept {}
  T await_resume() { return std::move(value); }
};

/// An in-memory stream, reads from `input` and writes to `output`
struct memory_stream {
  std::string input;
  std::string output;
  bool closed = false;

  ready<std::size_t> read_some(std::span<char> buffer)
  {
    std::size_t n = std::min(buffer.size(), input.size());
    std::memcpy(buffer.data(), input.data(), n);
    input.erase(0, n);
    return {n};
  }

  ready<std::size_t> write(std::span<const char> data)
  {
    output.append(data.data(), data.size());
    return {data.size()};
  }

  void close() { closed = true; }
};

struct memory_acceptor {
  ready<memory_stream> accept() { return {memory_stream{}}; }
  void close() {}
  bool is_open() const { return true; }
};

struct memory_connector {
  ready<memory_stream> connect(std::string_view, std::uint16_t) { return {memory_stream{}}; }
};

/// Awaitable through operator co_await
struct indirect {
  ready<std::size_t> operator co_await() const { return {0}; }
};

struct not_a_stream {
  void read_some(std::span<char>) {}
};

} // namespace

static_assert(Awaitable<ready<int>>);
static_assert(Awaitable<indirect>);
static_assert(AwaitableOf<indirect, std::size_t>);
static_assert(!Awaitable<int>);

static_assert(Stream<memory_stream>);
static_assert(!Stream<not_a_stream>);
static_assert(Acceptor<memory_acceptor>);
static_assert(Connector<memory_connector>);
static_assert(std::is_same_v<accepted_stream_t<memory_acceptor>, memory_stream>);
static_assert(std::is_same_v<connected_stream_t<memory_connector>, memory_stream>);

TEST_CASE("Transport concepts", "[webkit][transport]")
{
  memory_stream s;
  s.input = "hello";
  char buffer[3];

  REQUIRE(s.read_some(buffer).await_resume() == 3);
  REQUIRE(s.write(std::span<const char>("ok", 2)).await_resume() == 2);
  REQUIRE(s.output == "ok");
}
