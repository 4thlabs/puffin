cmake_minimum_required(VERSION 3.24)

file(GLOB_RECURSE HEADERS include/*.hpp)

puffin_declare_module(
  NAME
    rest
  HEADERS
    ${HEADERS}
  DEPS
    puffin::webkit
  FEATURES
    cxx_std_20
)

#
# JSON codec (puffin::rest_json), nlohmann::json
#
option(PUFFIN_REST_WITH_JSON "Build the JSON codec of puffin::rest" ON)

if(PUFFIN_REST_WITH_JSON)
  if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json QUIET)
  endif()

  # An installed nlohmann_json is a dependency of the exported target. A fetched one (tests and samples)
  # cannot be exported: it is linked by the tests and samples themselves
  set(PUFFIN_REST_JSON_DEP nlohmann_json::nlohmann_json)

  if(NOT nlohmann_json_FOUND)
    set(PUFFIN_REST_JSON_DEP "")

    if(NOT TARGET nlohmann_json::nlohmann_json AND (BUILD_TESTS OR BUILD_SAMPLES))
      CPMAddPackage("gh:nlohmann/json@3.12.0")
    endif()
  endif()

  if(TARGET nlohmann_json::nlohmann_json)
    puffin_declare_module(
      NAME
        rest_json
      DEPS
        puffin::rest
        ${PUFFIN_REST_JSON_DEP}
      FEATURES
        cxx_std_20
    )

    puffin_declare_tests(SOURCES tests/rest_test.cpp DEPS puffin::rest_json nlohmann_json::nlohmann_json)

    if(TARGET webkit_asio)
      puffin_declare_tests(SOURCES tests/rest_asio_test.cpp DEPS puffin::rest_json puffin::webkit_asio nlohmann_json::nlohmann_json)
    endif()
  else()
    message(STATUS "puffin: nlohmann_json not found, puffin::rest_json is not available")
  endif()
endif()
