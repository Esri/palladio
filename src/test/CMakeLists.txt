cmake_minimum_required(VERSION 3.13)

project(palladio_test CXX)

add_executable(${PROJECT_NAME}
		tests.cpp TestUtils.cpp TestCallbacks.h)

add_toolchain_definition(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE
		-DPLD_TEST_EXPORTS
		-DHOUDINI_CODEC_PATH="$<TARGET_FILE:palladio_codec>"
		-DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

if(PLD_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -std=c++11)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
		${palladio_codec_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        palladio
		palladio_codec)

pld_add_dependency_catch2(${PROJECT_NAME})
pld_add_dependency_boost(${PROJECT_NAME})
pld_add_dependency_prt(${PROJECT_NAME})
pld_add_dependency_houdini(${PROJECT_NAME})

# copy libraries next to test excutable so they can be found
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/bin
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_LIBRARIES} ${CMAKE_BINARY_DIR}/bin
	COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/lib
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_EXT_LIBRARIES} ${CMAKE_BINARY_DIR}/lib)
