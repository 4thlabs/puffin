cmake_minimum_required(VERSION 3.0)

file(GLOB_RECURSE HEADERS *.hpp)

puffin_declare_module(
  NAME
    events
  HEADERS
    ${HEADERS}
  FEATURES
    cxx_variadic_templates
    cxx_relaxed_constexpr
  DEPS
    puffin::common
)

puffin_declare_tests(SOURCES event_bus_test.cpp DEPS puffin::events)
