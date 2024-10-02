cmake_minimum_required(VERSION 3.24)

project(tdis-acts-root)


if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# Make C++20 a default
if(NOT "${CMAKE_CXX_STANDARD}")
  set(CMAKE_CXX_STANDARD 20)
endif()
message(STATUS "CMAKE_CXX_STANDARD = ${CMAKE_CXX_STANDARD}")


# Set the policy to NEW to use the environment variables in uppercase
cmake_policy(SET CMP0144 NEW)


#include(cmake/jana_plugin.cmake)

add_subdirectory(source/tdis)
