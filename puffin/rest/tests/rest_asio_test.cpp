#include "bank_api.hpp"

#include <puffin/async.hpp>
#include <puffin/async/adapter/asio.hpp>
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>
#include <puffin/webkit/adapter/asio.hpp>
#include <puffin/webkit/server/server.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

namespace pa = puffin::async;
namespace wk = puffin::webkit;
namespace rest = puffin::rest;

using namespace std::chrono_literals;

TEST_CASE("Rest api over asio", "[rest][asio]")
{
  ::asio::io_context io;
  pa::asio::executor executor {io};

  wk::asio::tcp_acceptor acceptor(io, 0, "127.0.0.1");
  bank::store db;
  wk::basic_server<> server;

  rest::mount<bank::v1>(server, bank::auth_service {db}, bank::users_service {db}, bank::cards_service {db},
                        rest::validators(bank::api_key_validator {"k3y"}, bank::bearer_validator {}));

  pa::co_spawn(executor, server.listen(acceptor));

  std::thread io_thread([&] { io.run_for(20s); });

  auto scenario = [&]() -> pa::async<bank::card> {
    // A Connector: the client wraps it in a webkit::basic_client
    rest::client<bank::v1, wk::asio::tcp_connector, rest::intercept::api_key> api(wk::asio::tcp_connector {io},
                                                                                  "127.0.0.1", acceptor.port());
    api.interceptor<rest::intercept::api_key>().key("k3y");

    bank::user ada = co_await api.call<bank::users::create>(bank::new_user {"Ada", "ada@bank.io"});
    bank::card c = co_await api.call<bank::cards::issue>(ada.id, bank::new_card {1000});
    co_return co_await api.call<bank::cards::freeze>(ada.id, c.id);
  };

  bank::card frozen = pa::sync_wait(executor, scenario());

  acceptor.close();
  io.stop();
  io_thread.join();

  REQUIRE(frozen.status == bank::card_status::frozen);
  REQUIRE(frozen.limit_cents == 1000);
}
