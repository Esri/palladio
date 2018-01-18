cmake_minimum_required(VERSION 3.8)

project(palladio_test CXX)

add_executable(${PROJECT_NAME}
		tests.cpp TestUtils.cpp TestCallbacks.h)

target_compile_options(${PROJECT_NAME} PRIVATE
        -std=c++11)

target_include_directories(${PROJECT_NAME} PRIVATE
		${palladio_codec_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        palladio
		palladio_codec
        CONAN_PKG::Catch)

target_compile_definitions(${PROJECT_NAME} PRIVATE
		-DHOUDINI_CODEC_PATH="$<TARGET_FILE:palladio_codec>"
		-DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

pld_add_dependency_prt(${PROJECT_NAME})
pld_add_dependency_houdini(${PROJECT_NAME})
