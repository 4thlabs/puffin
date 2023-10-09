file(GLOB_RECURSE HEADERS *.hpp)

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

puffin_declare_tests(SOURCES ioc_test.cpp DEPS puffin::ioc)
