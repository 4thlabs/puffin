cmake_minimum_required(VERSION 3.24)

file(GLOB_RECURSE HEADERS include/*.hpp)

puffin_declare_module(
  NAME
    imdb
  HEADERS
    ${HEADERS}
  FEATURES
    cxx_std_20
)

if (BUILD_TESTS)
  CPMAddPackage("gh:nlohmann/json@3.12.0")

  puffin_declare_tests(
    SOURCES
      tests/imdb_test.cpp
    DEPS
      puffin::imdb
      nlohmann_json::nlohmann_json
  )

endif()

