cmake_minimum_required(VERSION 3.8)

project(prt4houdini_tests CXX)

add_executable(${PROJECT_NAME} tests.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
        -std=c++11)

target_link_libraries(${PROJECT_NAME} PRIVATE
        CONAN_PKG::Catch
        CONAN_PKG::cesdk
        prt4houdini)

p4h_add_dependency_houdini(${PROJECT_NAME})