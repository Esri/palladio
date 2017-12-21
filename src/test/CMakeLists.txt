cmake_minimum_required(VERSION 3.8)

project(prt4houdini_tests CXX)

add_executable(${PROJECT_NAME} tests.cpp TestUtils.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
        -std=c++11)

target_include_directories(${PROJECT_NAME} PRIVATE
		${PRT_INCLUDE_PATH}
		${PRT4HOUDINI_CODEC_INCLUDE_PATH})

target_link_libraries(${PROJECT_NAME} PRIVATE
        prt4houdini
		prt4houdini_codec
        ${PRT_LINK_LIBRARIES}
        CONAN_PKG::Catch)

target_compile_definitions(${PROJECT_NAME} PRIVATE
		-DHOUDINI_CODEC_PATH="$<TARGET_FILE:prt4houdini_codec>"
		-DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

p4h_add_dependency_houdini(${PROJECT_NAME})
