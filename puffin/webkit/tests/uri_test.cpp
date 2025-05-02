#include <puffin/webkit/uri/uri.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE( "Url can be parsed or not", "[uri]" ) {
    SECTION("Url is valid") {
      pfn::uri valid_uri("http://www.google.fr/path/to/12#toto?param=&params2=45");
      REQUIRE(valid_uri.scheme() == "http");
      REQUIRE(valid_uri.host() == "www.google.fr");
      REQUIRE(valid_uri.path() == "/path/to/12");
      REQUIRE(valid_uri.fragment() == "toto");
      REQUIRE(valid_uri.query_string() == "param=&params2=45");
    }

     SECTION("Url is valid") {
       pfn::uri valid_uri("http://www.google.fr");
       REQUIRE(valid_uri.scheme() == "http");
       REQUIRE(valid_uri.host() == "www.google.fr");
       REQUIRE(valid_uri.path() == "/");
     }

    SECTION("Path is valid") {
      pfn::uri valid_uri("/path/to/12#toto?param=&params2=45");
      REQUIRE(valid_uri.path() == "/path/to/12");
      REQUIRE(valid_uri.fragment() == "toto");
      REQUIRE(valid_uri.query_string() == "param=&params2=45");
    }

    SECTION("Url is invalid") {
      REQUIRE_THROWS([&](){pfn::uri invalid_uri("htt://www.google.fr/path/to/12#toto?param=123");}());
    }
}
