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
#include <catch2/benchmark/catch_benchmark.hpp>

#include <iostream>
#include <nlohmann/json.hpp>
#include <puffin/imdb.hpp>

using namespace puffin::imdb;
using json = nlohmann::json;

json sample = R"(
  {
    "number": 1,
    "title": "My Title",
    "type": ["Standard"]
  }
)"_json;

json sample2 = R"(
  {
    "number": 2,
    "title": "Another My Title",
    "type": ["Hyperspace"]
  }
)"_json;

json sample3 = R"(
  {
    "number": 3,
    "title": "Your Title",
    "type": ["OP Promo"]
  }
)"_json;

auto f = [](const json& v) -> std::string {
  return "";
};

struct json_sort {
  std::string key = "";

  bool operator()(const json& v1, const json&v2) {
    return (v1.contains(key) && v2.contains(key)) ? v1[key] < v2[key] : false;
  }
};

struct json_contains {
  std::string key = "";
  std::string text = "";

  bool operator()(const json& v1) {
    return v1.contains(key) ? v1[key].get<std::string>().find(text) != std::string::npos : false;
  }
};

TEST_CASE("imbd") {

  puffin::imdb::in_memory_database<std::string, json> db;
  db.insert("doc", sample);
  db.insert("doc", sample3);
  db.insert("doc", sample2);

  SECTION("can insert data") {
    REQUIRE(db.size("doc") == 3);
    REQUIRE_NOTHROW(db.insert("doc", sample));
    REQUIRE(db.size("doc") == 4);
  }

  SECTION("Can access data") {
    REQUIRE_THROWS(db.row("doc", 4));

    REQUIRE_NOTHROW(db.row("doc", 0));

    auto& j = db.row("doc", 0);
    REQUIRE(j["number"] == 1);
  }

  SECTION("Can select data") {

    auto req = db.select().from("doc");
    auto result = req.execute();

    REQUIRE(result[0].get()["number"] == 1);
  }

  SECTION("Can select data with projection") {

   auto req = db.select([](const json& a) -> std::string {
                   return a["title"];
              }).from("doc");

   auto result = req.execute();
   REQUIRE(result[0].compare("My Title") == 0);
  }

  SECTION("Can filter data") {

    auto req = db.select()
                 .from("doc")
                 .where([](const json& j) {
                    return j["/type/0"_json_pointer].get<std::string>().compare("Standard") == 0;
                 });

    auto result = req.execute();
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].get()["number"] == 1);
  }

  SECTION("Can sort data") {

    auto req = db.select()
                 .from("doc")
                 .order_by([](const json& v1, const json& v2) {
                    return v1["number"] < v2["number"];
                 });

    auto result = req.execute();
    REQUIRE(result.size() == 3);
    REQUIRE(result[0].get()["number"] == 1);
    REQUIRE(result[1].get()["number"] == 2);
    REQUIRE(result[2].get()["number"] == 3);
  }

  SECTION("Can sort data and filter") {

    auto req = db.select()
                 .from("doc")
                 .where(json_contains{.key = "title", .text = "My"})
                 .order_by(json_sort{.key = "number"});

    auto result = req.execute();
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].get()["number"] == 1);
    REQUIRE(result[1].get()["number"] == 2);
  }
}

TEST_CASE("Imdb Benchmark") {
  BENCHMARK_ADVANCED("10k Json")(Catch::Benchmark::Chronometer meter) {
    puffin::imdb::in_memory_database<std::string, json> db;

    for (int i = 0; i < 10000; i++) {
      db.insert("doc", sample);
    }

    meter.measure([&]() {
      auto req = db.select()
                     .from("doc")
                     .where(json_contains{.key = "title", .text = "My"})
                     .order_by(json_sort{.key = "number"});
      return req.execute();
    });
  };
}
