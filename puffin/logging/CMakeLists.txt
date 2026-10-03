cmake_minimum_required(VERSION 4.1)

file(GLOB_RECURSE HEADERS *.hpp)

puffin_declare_module(
  NAME
    logging
  HEADERS
    ${HEADERS}
)