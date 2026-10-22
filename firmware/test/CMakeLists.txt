cmake_minimum_required(VERSION 3.16)
project(protocol_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Catch2 3 REQUIRED)

add_executable(test_protocol
    test_protocol.cpp
    ../src/protocol/wire.cpp
    ../src/protocol/messages.cpp
)
target_link_libraries(test_protocol PRIVATE Catch2::Catch2WithMain)

# Baked in at configure time rather than assumed as a relative path from the
# test binary - that path's depth (test/ vs test/build/ vs an IDE's own
# build dir) isn't something this file should have to track.
get_filename_component(PROTOCOL_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../protocol-fixtures" ABSOLUTE)
target_compile_definitions(test_protocol PRIVATE PROTOCOL_FIXTURES_DIR="${PROTOCOL_FIXTURES_DIR}")

include(CTest)
include(Catch)
catch_discover_tests(test_protocol)
