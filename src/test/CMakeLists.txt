cmake_minimum_required(VERSION 3.13)

add_executable(${TGT_TEST}
        tests.cpp TestUtils.cpp TestCallbacks.h)

pld_set_common_compiler_flags(${TGT_TEST})

target_compile_definitions(${TGT_TEST} PRIVATE
        -DPLD_TEST_EXPORTS
        -DHOUDINI_CODEC_PATH="$<TARGET_FILE:palladio_codec>"
        -DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

get_target_property(TGT_CODEC_SOURCE_DIR ${TGT_CODEC} SOURCE_DIR)
target_include_directories(${TGT_TEST} PRIVATE
        ${TGT_CODEC_SOURCE_DIR})

target_link_libraries(${TGT_TEST} PRIVATE
        palladio
        palladio_codec)

pld_add_dependency_catch2(${TGT_TEST})
pld_add_dependency_prt(${TGT_TEST})
pld_add_dependency_houdini(${TGT_TEST})

# copy libraries next to test executable so they can be found
add_custom_command(TARGET ${TGT_TEST} POST_BUILD
        COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PLD_PRT_LIBRARIES} ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/lib
        COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PLD_PRT_EXT_LIBRARIES} ${CMAKE_BINARY_DIR}/lib)
