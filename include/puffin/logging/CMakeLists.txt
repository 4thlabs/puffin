cmake_minimum_required(VERSION 3.0)

file(GLOB_RECURSE HEADERS *.hpp)

puffin_declare_module(
  NAME
    logging
  HEADERS
    ${HEADERS}
)