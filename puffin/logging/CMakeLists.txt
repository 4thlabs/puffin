file(GLOB_RECURSE HEADERS *.hpp)

puffin_declare_module(
  NAME
    logging
  HEADERS
    ${HEADERS}
)