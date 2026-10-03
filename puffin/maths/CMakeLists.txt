cmake_minimum_required(VERSION 4.1)

file(GLOB_RECURSE HEADERS include/**.hpp)

puffin_declare_module(
  NAME
    maths
  HEADERS
    ${HEADERS}
)

puffin_declare_tests(
  SOURCES 
    tests/vector3_test.cpp
    tests/matrix4_test.cpp
  DEPS 
    puffin::maths
)
