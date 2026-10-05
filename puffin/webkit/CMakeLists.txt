cmake_minimum_required(VERSION 3.5)

file(GLOB_RECURSE HEADERS include/**/*.hpp)

puffin_declare_module(
  NAME
    webkit
  HEADERS
    ${HEADERS}
  DEPS
    puffin::async
  FEATURES
    cxx_std_20
)

puffin_declare_tests(SOURCES tests/http_test.cpp DEPS puffin::webkit)
puffin_declare_tests(SOURCES tests/uri_test.cpp DEPS puffin::webkit)
