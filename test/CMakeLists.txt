find_package(Python)

add_executable(
    motionsdk_test
    test.cpp
    test_client.cpp
    test_format.cpp)

target_link_libraries(
    motionsdk_test
    PRIVATE
    motionsdk
    Catch2::Catch2WithMain)

# Run mock server to test client communication
# python test/mock_sdk_server.py motionsdk_test
add_test(
    NAME UnitTests
    COMMAND ${Python_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sdk_server.py
    $<TARGET_FILE:motionsdk_test>)
