cmake_minimum_required(VERSION 3.0)

project(puffin_common)

file(GLOB_RECURSE COMMON_HEADERS *.hpp)

puffin_declare_module(
  NAME
    common
  HEADERS
    ${COMMON_HEADERS}
  FEATURES
    cxx_constexpr
    cxx_variadic_templates
    cxx_lambdas
    cxx_raw_string_literals
)
