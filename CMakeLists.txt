cmake_minimum_required(VERSION 3.5)

project(puffin-cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PuffinHelpers.cmake)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_SAMPLES "Build samples" OFF)

add_subdirectory(puffin/common)
#add_subdirectory(include/puffin/logging)
add_subdirectory(puffin/events)
add_subdirectory(puffin/ioc)
add_subdirectory(puffin/maths)
add_subdirectory(puffin/async)
add_subdirectory(puffin/webkit)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
