cmake_minimum_required(VERSION 3.5)

file(GLOB_RECURSE HEADERS include/**.hpp)

puffin_declare_module(
  NAME
    async
  HEADERS
    ${HEADERS}
  FEATURES
    cxx_std_20
)

puffin_declare_tests(SOURCES tests/async_test.cpp DEPS puffin::async)
