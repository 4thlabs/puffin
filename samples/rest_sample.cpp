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
#include <puffin/rest.hpp>
#include <puffin/rest/json.hpp>
#include <puffin/webkit.hpp>
#include <puffin/webkit/adapter/asio.hpp>

#include <asio/signal_set.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pa = puffin::async;
namespace wk = puffin::webkit;
namespace rest = puffin::rest;

//
// The api, shared by the server and the client
//

struct note {
  int id = 0;
  std::string text;
  std::vector<std::string> tags;
};

struct new_note {
  std::string text;
  std::vector<std::string> tags;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(note, id, text, tags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(new_note, text, tags)

namespace notes {
using namespace puffin::rest;

using list = endpoint<"notes.list", GET, "/", query<"tag", std::optional<std::string>>, returns<std::vector<note>>>;
using get = endpoint<"notes.get", GET, "/{id:int}", returns<note>>;
using create = endpoint<"notes.create", POST, "/", body<new_note>, returns<note, status::created>>;
using remove = endpoint<"notes.remove", DELETE, "/{id:int}">;

using api = rest::api<"/notes", list, get, create, remove>;
} // namespace notes

namespace health {
using namespace puffin::rest;

using ping = endpoint<"health.ping", GET, "/ping", returns<std::string>>;

using api = rest::api<"/health", ping>;
} // namespace health

using v1 = rest::api<"/api/v1", health::api, rest::with<rest::security<rest::api_key<"X-Api-Key">>, notes::api>>;

//
// Server side
//

class notes_service {
public:
  std::vector<note> operator()(notes::list, std::optional<std::string> tag)
  {
    std::vector<note> result;

    for (auto& [id, n] : notes_) {
      if (!tag || std::find(n.tags.begin(), n.tags.end(), *tag) != n.tags.end())
        result.push_back(n);
    }

    return result;
  }

  note operator()(notes::get, int id)
  {
    auto it = notes_.find(id);

    if (it == notes_.end())
      throw rest::http_error(rest::status::not_found, "no note " + std::to_string(id));

    return it->second;
  }

  note operator()(notes::create, new_note n)
  {
    note created {next_id_++, std::move(n.text), std::move(n.tags)};
    notes_[created.id] = created;
    return created;
  }

  void operator()(notes::remove, int id) { notes_.erase(id); }

  std::string operator()(health::ping) { return "pong"; }

private:
  std::map<int, note> notes_;
  int next_id_ = 1;
};

/// A webkit middleware logging every call with its endpoint and status
struct access_log {
  template<typename Context>
  pa::async<void> around(Context& ctx, wk::next_handler next)
  {
    co_await next();
    std::cout << (ctx.route() ? ctx.route()->name : "-") << " " << ctx.request().target() << " -> "
              << ctx.response().status_code() << std::endl;
  }
};

//
// Server side
//

using server_type = wk::basic_server<access_log>;

void mount_api(server_type& server)
{
  rest::mount<v1>(server, notes_service {},
                  rest::validators([](const rest::api_key_value& key) { return key.value == "secret-key"; }));
}

//
// Client side, over any webkit Connector
//

using notes_client = rest::client<v1, wk::interceptors::retry>;

pa::async<void> demo(wk::any_connector connector, std::uint16_t port)
{
  notes_client api(std::move(connector), "127.0.0.1", port);
  api.credentials<rest::api_key<"X-Api-Key">>("secret-key");

  std::cout << "ping: " << co_await api.call<health::ping>() << std::endl;

  co_await api.call<notes::create>(new_note {"Write the rest DSL", {"puffin", "todo"}});
  co_await api.call<notes::create>(new_note {"Buy bread", {"home"}});

  for (const note& n : co_await api.call<notes::list>("puffin"))
    std::cout << "note " << n.id << ": " << n.text << std::endl;

  auto missing = co_await api.try_call<notes::get>(42);

  if (!missing)
    std::cout << "note 42: " << missing.error().status() << " " << missing.error().body() << std::endl;
}

// Serves the notes api, after a short demo of the client:
//   curl -H 'X-Api-Key: secret-key' http://127.0.0.1:8080/api/v1/notes?tag=home
// main picks the runtime, asio here: everything above only uses puffin.
int main(int argc, char** argv)
{
  std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::atoi(argv[1])) : 8080;

  ::asio::io_context io;
  pa::asio::executor executor {io};

  server_type server;
  mount_api(server);

  wk::asio::tcp_acceptor acceptor(io, port);
  std::cout << "Listening on http://0.0.0.0:" << acceptor.port() << std::endl;

  pa::co_spawn(executor, server.listen(acceptor), [](std::exception_ptr e) {
    if (e)
      std::cerr << "Server stopped with an error" << std::endl;
  });

  pa::co_spawn(executor, demo(wk::asio::tcp_connector {io}, acceptor.port()), [](std::exception_ptr e) {
    if (e)
      std::cerr << "Demo failed" << std::endl;
  });

  ::asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&](auto, int) { acceptor.close(); });

  io.run();
  return 0;
}
