cmake_minimum_required(VERSION 3.8)

project(prt4houdini_tests CXX)

add_executable(${PROJECT_NAME} tests.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
        -std=c++11)

target_include_directories(${PROJECT_NAME} PRIVATE
		${PRT_INCLUDE_PATH}
		${PRT4HOUDINI_CODEC_INCLUDE_PATH})

target_link_libraries(${PROJECT_NAME} PRIVATE
        prt4houdini
        ${PRT_LINK_LIBRARIES}
        CONAN_PKG::Catch)

p4h_add_dependency_houdini(${PROJECT_NAME})