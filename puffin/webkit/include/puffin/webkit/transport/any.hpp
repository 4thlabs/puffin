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

#ifndef PUFFIN_WEBKIT_TRANSPORT_ANY_HPP
#define PUFFIN_WEBKIT_TRANSPORT_ANY_HPP

#include <puffin/async/async.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace puffin {
namespace webkit {

/**
 * @brief Any Stream, behind one virtual call per operation.
 *
 * Owns its stream: a moved-from any_stream throws std::logic_error. Only basic_client holds one in practice.
 */
class any_stream {
public:
  template<Stream S>
    requires(!std::same_as<std::decay_t<S>, any_stream>)
  any_stream(S stream)
      : impl_(std::make_unique<model<S>>(std::move(stream)))
  {}

  async::async<std::size_t> read_some(std::span<char> buffer) { return stream().read_some(buffer); }
  async::async<std::size_t> write(std::span<const char> data) { return stream().write(data); }
  void close() { stream().close(); }

private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual async::async<std::size_t> read_some(std::span<char> buffer) = 0;
    virtual async::async<std::size_t> write(std::span<const char> data) = 0;
    virtual void close() = 0;
  };

  template<typename S>
  struct model final : concept_t {
    explicit model(S s)
        : stream(std::move(s))
    {}

    async::async<std::size_t> read_some(std::span<char> buffer) override
    {
      co_return co_await stream.read_some(buffer);
    }

    async::async<std::size_t> write(std::span<const char> data) override { co_return co_await stream.write(data); }
    void close() override { stream.close(); }

    S stream;
  };

  concept_t& stream() const
  {
    if (!impl_)
      throw std::logic_error("any_stream: moved from");

    return *impl_;
  }

  std::unique_ptr<concept_t> impl_;
};

/**
 * @brief Any Connector, its streams being any_stream. Application code taking an any_connector does not depend on
 *        the runtime, only the caller picks it (the constructor is implicit for that):
 *
 *   void run(rest::client<v1, webkit::any_connector>& api);
 *
 *   rest::client<v1, webkit::any_connector> api(webkit::asio::tcp_connector { io }, "example.com", 80);
 *
 * Never empty: copies share the wrapped connector, and moving copies, so that a reference to an any_connector
 * stays usable whatever happens to the object it refers to. Host and buffers must outlive the operations, as for
 * any Connector and Stream.
 */
class any_connector {
public:
  template<Connector C>
    requires(!std::same_as<std::decay_t<C>, any_connector>)
  any_connector(C connector)
      : impl_(std::make_shared<model<C>>(std::move(connector)))
  {}

  // No move operations: moving copies, the source keeps its connector
  any_connector(const any_connector&) = default;
  any_connector& operator=(const any_connector&) = default;
  ~any_connector() = default;

  async::async<any_stream> connect(std::string_view host, std::uint16_t port) { return impl_->connect(host, port); }

private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual async::async<any_stream> connect(std::string_view host, std::uint16_t port) = 0;
  };

  template<typename C>
  struct model final : concept_t {
    explicit model(C c)
        : connector(std::move(c))
    {}

    async::async<any_stream> connect(std::string_view host, std::uint16_t port) override
    {
      co_return any_stream(co_await connector.connect(host, port));
    }

    C connector;
  };

  std::shared_ptr<concept_t> impl_;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_TRANSPORT_ANY_HPP
