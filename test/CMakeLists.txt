find_package(Python)

add_executable(
    test_MotionSDK
    test.cpp
    test_client.cpp
    test_format.cpp)

target_link_libraries(test_MotionSDK PRIVATE MotionSDK)

add_test(
    NAME test_MotionSDK
    COMMAND ${Python_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sdk_server.py
    $<TARGET_FILE:test_MotionSDK>)
