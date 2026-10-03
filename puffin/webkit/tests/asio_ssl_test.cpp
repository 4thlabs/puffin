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

#include <puffin/async.hpp>
#include <puffin/async/adapter/asio.hpp>
#include <puffin/webkit/adapter/asio_ssl.hpp>
#include <puffin/webkit/client/client.hpp>
#include <puffin/webkit/server/server.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace pa = puffin::async;
namespace wk = puffin::webkit;

using namespace std::chrono_literals;

namespace {

// Self-signed certificate for localhost / 127.0.0.1, for tests only
constexpr const char* test_certificate = R"(-----BEGIN CERTIFICATE-----
MIIBmjCCAUGgAwIBAgIUdVEoc76Dd35yFuPptTwY0hIGnHQwCgYIKoZIzj0EAwIw
FDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MTAwMzEwNDA0OFoYDzIxMjYwOTA5
MTA0MDQ4WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAARTTT/1Dc7y2OGKDSEC4n9c5Z8ijyQrJ+qzLMHASStBycsPKCWE289k
yUy/x4UdPshN3qU2jLzYat1NZs/J/dSZo28wbTAdBgNVHQ4EFgQUsHWmPamW3clV
Sw9HUbVigddh+aowHwYDVR0jBBgwFoAUsHWmPamW3clVSw9HUbVigddh+aowDwYD
VR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgglsb2NhbGhvc3SHBH8AAAEwCgYIKoZI
zj0EAwIDRwAwRAIgPOATH9WClroK/LOkHP5AS83v+2yL29Lf0x83PlGO4YkCIHIl
MsuLg27Pm4AdVADnBtaCFVdqY4YEM4dZtMVvpY0b
-----END CERTIFICATE-----
)";

constexpr const char* test_private_key = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgwpi1DYL2ffuzM6N6
+rWD1Z1jjFuhOX1ASHlvRDoYZbOhRANCAARTTT/1Dc7y2OGKDSEC4n9c5Z8ijyQr
J+qzLMHASStBycsPKCWE289kyUy/x4UdPshN3qU2jLzYat1NZs/J/dSZ
-----END PRIVATE KEY-----
)";

} // namespace

TEST_CASE("Server and client over TLS", "[webkit][asio][ssl]")
{
  ::asio::io_context io;
  pa::asio::executor executor{io};

  ::asio::ssl::context server_context(::asio::ssl::context::tls_server);
  server_context.use_certificate_chain(::asio::buffer(std::string_view(test_certificate)));
  server_context.use_private_key(::asio::buffer(std::string_view(test_private_key)), ::asio::ssl::context::pem);

  ::asio::ssl::context client_context(::asio::ssl::context::tls_client);
  client_context.add_certificate_authority(::asio::buffer(std::string_view(test_certificate)));

  ::asio::ssl::context untrusting_context(::asio::ssl::context::tls_client);

  wk::asio::tls_acceptor acceptor(io, 0, server_context, "127.0.0.1");

  wk::basic_server<> server;
  server.get("/secure", [](auto& ctx) { ctx.response().body("secret"); });

  pa::co_spawn(executor, server.listen(acceptor));
  std::thread io_thread([&] { io.run_for(20s); });

  auto scenario = [&]() -> pa::async<std::string> {
    wk::basic_client client(wk::asio::tls_connector{io, client_context}, "localhost", acceptor.port());
    std::string body = (co_await client.get("/secure")).body();
    body += (co_await client.get("/secure")).body();
    co_return body;
  };

  REQUIRE(pa::sync_wait(executor, scenario()) == "secretsecret");

  // A client that does not trust the certificate fails the handshake, the server keeps running
  auto untrusted = [&]() -> pa::async<bool> {
    wk::basic_client client(wk::asio::tls_connector{io, untrusting_context}, "localhost", acceptor.port());

    try {
      co_await client.get("/secure");
    } catch (const std::exception&) {
      co_return true;
    }

    co_return false;
  };

  REQUIRE(pa::sync_wait(executor, untrusted()));
  REQUIRE(pa::sync_wait(executor, scenario()) == "secretsecret");

  pa::sync_wait(executor, [&]() -> pa::async<void> {
    acceptor.close();
    co_return;
  }());

  io_thread.join();
}
