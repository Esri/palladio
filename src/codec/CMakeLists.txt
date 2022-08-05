cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### build target

add_library(${TGT_CODEC} SHARED
        CodecMain.cpp
        encoder/HoudiniEncoder.cpp
        encoder/HoudiniCallbacks.h)

pld_set_common_compiler_flags(${TGT_CODEC})
pld_set_prtx_compiler_flags(${TGT_CODEC})

if (PLD_LINUX)
    if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
        target_compile_options(${TGT_CODEC} PRIVATE -O3 -flto)
        target_compile_definitions(${TGT_CODEC} PRIVATE -DNDEBUG)
    elseif (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
        target_compile_options(${TGT_CODEC} PRIVATE -O3 -ggdb -pg)
        target_compile_definitions(${TGT_CODEC} PRIVATE -DNDEBUG)
    elseif (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        target_compile_options(${TGT_CODEC} PRIVATE -O0 -ggdb -pg)
        target_compile_definitions(${TGT_CODEC} PRIVATE -DDEBUG)
    endif ()

endif ()


### dependencies

pld_add_dependency_prt(${TGT_CODEC})


### install target

install(TARGETS ${TGT_CODEC}
        RUNTIME DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}"
        LIBRARY DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")
