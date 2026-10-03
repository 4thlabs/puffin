cmake_minimum_required(VERSION 3.5)

file(GLOB_RECURSE HEADERS include/*.hpp)

puffin_declare_module(
  NAME
    async
  HEADERS
    ${HEADERS}
  FEATURES
    cxx_std_20
)

puffin_declare_tests(SOURCES tests/async_test.cpp DEPS puffin::async)

#
# asio adapter (puffin::async_asio), standalone asio or Boost.Asio
#
option(PUFFIN_ASYNC_WITH_ASIO "Build the asio adapter of puffin::async when asio is found" ON)
option(PUFFIN_ASYNC_USE_BOOST_ASIO "Use Boost.Asio instead of standalone asio for the asio adapter" OFF)

if(PUFFIN_ASYNC_WITH_ASIO)
  if(PUFFIN_ASYNC_USE_BOOST_ASIO)
    find_package(Boost QUIET)

    if(Boost_FOUND)
      find_package(Threads REQUIRED)

      puffin_declare_module(
        NAME
          async_asio
        DEPS
          puffin::async
          Boost::headers
          Threads::Threads
        FEATURES
          cxx_std_20
      )

      target_compile_definitions(async_asio INTERFACE PUFFIN_ASYNC_USE_BOOST_ASIO)
    endif()
  else()
    find_path(PUFFIN_ASIO_INCLUDE_DIR asio.hpp)

    if(PUFFIN_ASIO_INCLUDE_DIR)
      find_package(Threads REQUIRED)

      puffin_declare_module(
        NAME
          async_asio
        DEPS
          puffin::async
          Threads::Threads
        FEATURES
          cxx_std_20
      )

      target_include_directories(async_asio SYSTEM INTERFACE $<BUILD_INTERFACE:${PUFFIN_ASIO_INCLUDE_DIR}>)
      target_compile_definitions(async_asio INTERFACE ASIO_STANDALONE)

      puffin_declare_tests(SOURCES tests/asio_test.cpp DEPS puffin::async_asio)
    endif()
  endif()

  if(NOT TARGET async_asio)
    message(STATUS "puffin: asio not found, puffin::async_asio is not available")
  endif()
endif()

#
# Qt adapter (puffin::async_qt), Qt 6
#
option(PUFFIN_ASYNC_WITH_QT "Build the Qt adapter of puffin::async when Qt 6 is found" ON)

if(PUFFIN_ASYNC_WITH_QT)
  find_package(Qt6 QUIET COMPONENTS Core)

  if(Qt6Core_FOUND)
    puffin_declare_module(
      NAME
        async_qt
      DEPS
        puffin::async
        Qt6::Core
      FEATURES
        cxx_std_20
    )

    puffin_declare_tests(SOURCES tests/qt_test.cpp DEPS puffin::async_qt)
  else()
    message(STATUS "puffin: Qt 6 not found, puffin::async_qt is not available")
  endif()
endif()
