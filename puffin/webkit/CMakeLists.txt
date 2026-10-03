cmake_minimum_required(VERSION 3.24)

file(GLOB_RECURSE HEADERS include/*.hpp)

puffin_declare_module(
  NAME
    webkit
  HEADERS
    ${HEADERS}
  DEPS
    puffin::async
  FEATURES
    cxx_std_20
)

puffin_declare_tests(
  SOURCES
    tests/uri_test.cpp
    tests/http_test.cpp
    tests/parser_test.cpp
    tests/server_test.cpp
    tests/transport_test.cpp
    tests/server_io_test.cpp
  DEPS
    puffin::webkit
)

#
# asio transport (puffin::webkit_asio), and TLS over OpenSSL (puffin::webkit_asio_ssl)
#
option(PUFFIN_WEBKIT_WITH_SSL "Build the TLS transport of puffin::webkit when OpenSSL is found" ON)

if(TARGET async_asio)
  puffin_declare_module(
    NAME
      webkit_asio
    DEPS
      puffin::webkit
      puffin::async_asio
    FEATURES
      cxx_std_20
  )

  puffin_declare_tests(SOURCES tests/asio_test.cpp DEPS puffin::webkit_asio)

  if(PUFFIN_WEBKIT_WITH_SSL)
    find_package(OpenSSL QUIET)

    if(OpenSSL_FOUND)
      puffin_declare_module(
        NAME
          webkit_asio_ssl
        DEPS
          puffin::webkit_asio
          OpenSSL::SSL
          OpenSSL::Crypto
        FEATURES
          cxx_std_20
      )

      puffin_declare_tests(SOURCES tests/asio_ssl_test.cpp DEPS puffin::webkit_asio_ssl)
    else()
      message(STATUS "puffin: OpenSSL not found, puffin::webkit_asio_ssl is not available")
    endif()
  endif()
else()
  message(STATUS "puffin: puffin::async_asio not available, puffin::webkit_asio is not available")
endif()

#
# Qt transport (puffin::webkit_qt), Qt 6 Network
#
if(TARGET async_qt)
  find_package(Qt6 QUIET COMPONENTS Network)

  if(Qt6Network_FOUND)
    puffin_declare_module(
      NAME
        webkit_qt
      DEPS
        puffin::webkit
        puffin::async_qt
        Qt6::Network
      FEATURES
        cxx_std_20
    )

    puffin_declare_tests(SOURCES tests/qt_test.cpp DEPS puffin::webkit_qt)
  else()
    message(STATUS "puffin: Qt 6 Network not found, puffin::webkit_qt is not available")
  endif()
else()
  message(STATUS "puffin: puffin::async_qt not available, puffin::webkit_qt is not available")
endif()
