cmake_minimum_required(VERSION 3.14)

file(GLOB_RECURSE COMMON_HEADERS include/**.hpp)

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
