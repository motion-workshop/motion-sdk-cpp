# Build the MotionSDK library, unit tests, and example apps with CMake
cmake_minimum_required(VERSION 3.5)

project(MotionSDK LANGUAGES CXX)

# MotionSDK is a C++11 library
add_library(MotionSDK src/Client.cpp src/Format.cpp)
target_compile_features(MotionSDK PUBLIC cxx_std_11)
target_include_directories(MotionSDK PUBLIC include)

if(WIN32)
    target_link_libraries(MotionSDK PUBLIC ws2_32)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
    add_subdirectory(example)
endif()
