cmake_minimum_required(VERSION 3.25)
project(log-replay VERSION 0.1.0 LANGUAGES CXX
    DESCRIPTION "Deterministic flight data recorder and bit-exact replay for avionics research")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_library(logrep INTERFACE)
target_include_directories(logrep INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

option(LOGREP_BUILD_TESTS "Build tests" ON)
if(LOGREP_BUILD_TESTS)
    enable_testing()
    add_executable(test_logrep tests/test_logrep.cpp)
    target_link_libraries(test_logrep PRIVATE logrep)
    add_test(NAME test_logrep COMMAND test_logrep)
endif()
