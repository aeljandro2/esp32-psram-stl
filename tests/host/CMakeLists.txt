cmake_minimum_required(VERSION 3.16)
project(psram_stl_host_tests CXX)

# Runs the library on a PC against a simulated ESP32 heap:
#   cmake -S tests/host -B build/host && cmake --build build/host && ./build/host/host_tests

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PSRAM_STL_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)

add_executable(host_tests test_main.cpp fake_heap.cpp)
target_include_directories(host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/shim)
target_compile_definitions(host_tests PRIVATE PSRAM_STL_HOST)
target_compile_options(host_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror -Wno-sign-conversion)

if(PSRAM_STL_SANITIZE)
  target_compile_options(host_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_link_options(host_tests PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
