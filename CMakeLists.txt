cmake_minimum_required(VERSION 3.10)
project(MessagePool)

set(CMAKE_CXX_STANDARD 17)

# Include directory
include_directories(include)

# Google Test
find_package(GTest REQUIRED)

# Test executable
add_executable(message_pool_tests
    src/message_pool_tests.cpp
    include/message_pool.h
)

target_link_libraries(message_pool_tests
    GTest::GTest
    GTest::Main
)

# Enable testing
enable_testing()
add_test(NAME message_pool_tests
         COMMAND message_pool_tests)
