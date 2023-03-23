# Minimum version for target_sources(... FILE_SET HEADERS ...)
cmake_minimum_required(VERSION 3.23)

project(motionsdk)

add_library(motionsdk src/Client.cpp src/Format.cpp)
add_library(motionsdk::motionsdk ALIAS motionsdk)

target_compile_features(motionsdk PUBLIC cxx_std_11)
target_include_directories(motionsdk PUBLIC include)

if(WIN32)
    target_link_libraries(motionsdk PUBLIC ws2_32)
endif()

target_sources(
    motionsdk PUBLIC FILE_SET HEADERS
    BASE_DIRS include
    FILES
    include/Client.hpp
    include/Format.hpp
    include/LuaConsole.hpp
    include/detail/endian.hpp
    include/detail/endian_to_native.hpp)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)

    install(TARGETS motionsdk FILE_SET HEADERS)
    
    add_subdirectory(example)
endif()

find_package(Catch2)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING AND Catch2_FOUND)
    add_subdirectory(test)
endif()
