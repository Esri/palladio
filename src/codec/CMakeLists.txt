cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### build target

add_library(${TGT_CODEC} SHARED
        CodecMain.cpp
        encoder/HoudiniEncoder.cpp
        encoder/HoudiniCallbacks.h)

pld_set_common_compiler_flags(${TGT_CODEC})
pld_set_prtx_compiler_flags(${TGT_CODEC})


### dependencies

pld_add_dependency_prt(${TGT_CODEC})


### install target

install(TARGETS ${TGT_CODEC}
        RUNTIME DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}"
        LIBRARY DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")
