cmake_minimum_required(VERSION 3.0)

project(puffin_maths)

file(GLOB_RECURSE HEADERS include/*.hpp)

puffin_declare_module(
  NAME
    maths
  HEADERS
    ${HEADERS}
)

puffin_declare_tests(
  SOURCES 
    vector3_test.cpp
    matrix4_test.cpp 
  DEPS 
    puffin::maths
)
