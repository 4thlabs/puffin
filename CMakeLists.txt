cmake_minimum_required(VERSION 3.11)

project(puffin-cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PuffinHelpers.cmake)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_SAMPLES "Build samples" OFF)

add_subdirectory(include/puffin/common)
add_subdirectory(include/puffin/logging)
add_subdirectory(include/puffin/events)
add_subdirectory(include/puffin/ioc)
add_subdirectory(include/puffin/maths)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
