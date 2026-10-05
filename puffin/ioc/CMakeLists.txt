cmake_minimum_required(VERSION 3.0)

file(GLOB_RECURSE HEADERS include/**.hpp)

puffin_declare_module(
  NAME
    ioc
  HEADERS
    ${HEADERS}
  FEATURES
    cxx_std_17
  DEPS
    puffin::common
)

puffin_declare_tests(SOURCES tests/ioc_test.cpp DEPS puffin::ioc)
