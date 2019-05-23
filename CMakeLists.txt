#
# @file  CMakeLists.txt
# @brief Implements the MotionSDK library, unit tests, and usage example apps as
#        a CMake project.
#
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(
  motion-sdk-cpp
  VERSION 3.0
  LANGUAGES CXX)

#
# Global properties to enable C++11
#
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# MotionSDK static library
#
add_library(
  MotionSDK
  src/Client.cpp
  src/Format.cpp
  include/Client.hpp
  include/Format.hpp
  include/LuaConsole.hpp)

# Public include path
target_include_directories(MotionSDK PUBLIC include)
# Public library dependencies
if(WIN32)
  target_link_libraries(MotionSDK PUBLIC ws2_32)
endif()

#
# Unit test app
#
add_executable(
  test_MotionSDK
  test/test.cpp
  test/test_client.cpp
  test/test_format.cpp)

target_link_libraries(test_MotionSDK PRIVATE MotionSDK)

#
# Example apps
#
# Basic example, from the project README docs
add_executable(
  example_MotionSDK
  example/example.cpp)

target_link_libraries(example_MotionSDK PRIVATE MotionSDK)

# More in depth example, stream to CSV format
add_executable(
  example_stream_csv
  example/example_stream_csv.cpp)

target_link_libraries(example_stream_csv PRIVATE MotionSDK)

#
# Run unit tests
#
enable_testing()
add_test(NAME test_MotionSDK COMMAND test_MotionSDK)
