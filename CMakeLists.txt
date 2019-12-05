cmake_minimum_required(VERSION 3.11)

project(puffin-cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PuffinHelpers.cmake)

option(BUILD_TESTS "Build tests" on)
option(BUILD_SAMPLES "Build samples" on)

option(PFN_WITH_HTTP "Option to include the http module" off)

add_subdirectory(puffin/common)
add_subdirectory(puffin/logging)
add_subdirectory(puffin/events)
add_subdirectory(puffin/ioc)
add_subdirectory(puffin/maths)

if(PFN_WITH_HTTP)
  add_subdirectory(puffin/http)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
