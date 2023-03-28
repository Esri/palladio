cmake_minimum_required(VERSION 3.13)

get_target_property(TGT_PALLADIO_SOURCE_DIR ${TGT_PALLADIO} SOURCE_DIR)
get_target_property(TGT_CODEC_SOURCE_DIR ${TGT_CODEC} SOURCE_DIR)
get_target_property(TGT_CODEC_BINARY_DIR ${TGT_CODEC} BINARY_DIR)

add_executable(${TGT_TEST}
        tests.cpp
        TestUtils.cpp
        TestCallbacks.h
        ${TGT_PALLADIO_SOURCE_DIR}/Utils.cpp
        ${TGT_PALLADIO_SOURCE_DIR}/PRTContext.cpp
        ${TGT_PALLADIO_SOURCE_DIR}/LogHandler.cpp
        ${TGT_PALLADIO_SOURCE_DIR}/ResolveMapCache.cpp
        ${TGT_CODEC_SOURCE_DIR}/encoder/HoudiniEncoder.cpp)

pld_set_common_compiler_flags(${TGT_TEST})
pld_set_prtx_compiler_flags(${TGT_TEST}) # we directly link to codecs code

target_compile_definitions(${TGT_TEST} PRIVATE
        -DPLD_TEST_EXPORTS
        -DTEST_RUN_PRT_EXT_DIR="${PRT_EXTENSION_PATH}" # the built-in extension libraries of PRT
        -DTEST_RUN_CODEC_EXT_DIR="${TGT_CODEC_BINARY_DIR}" # our palladio codec
        -DTEST_DATA_PATH="${CMAKE_CURRENT_LIST_DIR}/data")

target_include_directories(${TGT_TEST} PRIVATE
        ${TGT_PALLADIO_SOURCE_DIR}
        ${TGT_CODEC_SOURCE_DIR})

if (PLD_LINUX)
    target_link_libraries(${TGT_TEST} PRIVATE dl)
endif ()

pld_add_dependency_catch2(${TGT_TEST})
pld_add_dependency_prt(${TGT_TEST})

if (PLD_WINDOWS)
    # copy dependency libraries next to test executable so they can be found on Windows (no need to change PATH)
    add_custom_command(TARGET ${TGT_TEST} POST_BUILD
            COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PLD_PRT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR})
endif ()

add_test(NAME ${TGT_TEST} COMMAND ${TGT_TEST} -r junit -o  ${CMAKE_CURRENT_BINARY_DIR}/${TGT_TEST}_report.xml)

if (NOT PLD_GEN_VISUAL_STUDIO)
    add_custom_target(build_and_run_tests
            # first we build our test target
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${TGT_TEST}
            # then we run the ctest synthetic 'test' target which runs the tests
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Build and run the tests"
            VERBATIM)
endif()