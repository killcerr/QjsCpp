cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(quickjs)

project(qjs_cpp)

add_library(qjs_cpp INTERFACE)
target_include_directories(qjs_cpp INTERFACE include/)
target_link_libraries(qjs_cpp INTERFACE qjs)

option(QJS_CPP_TEST "Whether to compile test code" ON)

if(QJS_CPP_TEST)
    add_executable(qjs_cpp_test test.cpp)
    target_link_libraries(qjs_cpp_test PUBLIC qjs_cpp)
endif()
