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

#ifndef PUFFIN_REST_TRANSPORT_HPP
#define PUFFIN_REST_TRANSPORT_HPP

#include <puffin/async/async.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/detail/pointer.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>
#include <puffin/webkit/transport/any.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace puffin {
namespace rest {

/**
 * @brief What a rest client sends its requests through: webkit::basic_client, local_transport...
 */
template<typename T>
concept Transport = requires(T& t, webkit::request req) {
  { t.request(std::move(req)) } -> std::same_as<async::async<webkit::response>>;
};

/**
 * @brief A transport calling a webkit server in process, without any I/O. For tests, or to call an api
 *        served by the same program.
 */
template<typename Server>
class local_transport {
public:
  explicit local_transport(Server& server)
      : server_(server)
  {}

  async::async<webkit::response> request(webkit::request req)
  {
    webkit::response res;
    co_await server_.handle(req, res);
    co_return res;
  }

private:
  Server& server_;
};

/**
 * @brief Any Transport, behind one virtual call per request, so that rest::client does not depend on it:
 *
 *   any_transport(webkit::asio::tcp_connector { io }, "example.com", 80);  // a webkit::basic_client<any_connector>
 *   any_transport(local_transport(server));                                // any Transport
 *
 * The constructors are implicit for that. A moved-from any_transport throws std::logic_error.
 */
class any_transport {
public:
  template<Transport T>
    requires(!std::same_as<std::decay_t<T>, any_transport>)
  any_transport(T transport)
      : impl_(std::make_unique<model<T>>(std::move(transport)))
  {}

  /// Over a webkit::basic_client to host:port, through any Connector
  template<webkit::Connector C>
  any_transport(C connector, std::string host, std::uint16_t port, webkit::client_options options = {})
      : impl_(std::make_unique<model<webkit::basic_client<webkit::any_connector>>>(
            webkit::any_connector(std::move(connector)), std::move(host), port, std::move(options)))
  {}

  async::async<webkit::response> request(webkit::request req) { return transport().request(std::move(req)); }

private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual async::async<webkit::response> request(webkit::request req) = 0;
  };

  template<typename T>
  struct model final : concept_t {
    template<typename... A>
    explicit model(A&&... args)
        : transport(std::forward<A>(args)...)
    {}

    async::async<webkit::response> request(webkit::request req) override { return transport.request(std::move(req)); }

    T transport;
  };

  concept_t& transport() const { return webkit::detail::checked(impl_, "any_transport: moved from"); }

  std::unique_ptr<concept_t> impl_;
};

} // namespace rest
} // namespace puffin

#endif // PUFFIN_REST_TRANSPORT_HPP
