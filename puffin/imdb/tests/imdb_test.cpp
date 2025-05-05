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

#include <catch2/catch_test_macros.hpp>
#include <puffin/imdb.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace puffin::imdb;
using json = nlohmann::json;

json sample = R"(
  {
    "number": 1,
    "title": "My Title"
    "type": ["Standard", "Hyperspace"]
  }
)"_json;

json sample2 = R"(
  {
    "number": 2,
    "title": "Another title"
    "type": ["Standard", "Hyperspace"]
  }
)"_json;

auto f = [](const json& v) -> std::string {
  return "";
};

TEST_CASE("imbd") {

  puffin::imdb::in_memory_database<std::string, json> db;

  SECTION("can insert data") {
    REQUIRE(db.size("doc") == 0);
    REQUIRE_NOTHROW(db.insert("doc", sample));
    REQUIRE(db.size("doc") == 1);
  }

  SECTION("Can access data") {
    REQUIRE_THROWS(db.row("doc", 0));

    REQUIRE_NOTHROW(db.insert("doc", sample));

    REQUIRE_NOTHROW(db.row("doc", 0));

    auto& j = db.row("doc", 0);
    REQUIRE(j["number"] == 1);
  }


  SECTION("Can select data") {
    db.select([](const json& v) -> std::string {
        return v["title"];
      })
      .from("doc");
  }
}
