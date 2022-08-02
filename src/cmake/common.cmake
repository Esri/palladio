cmake_minimum_required(VERSION 3.13)


### platform configuration

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PLD_WINDOWS 1)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PLD_LINUX 1)
endif ()


### toolchain configuration

function(pld_set_common_compiler_flags TGT)
    set_target_properties(${TGT} PROPERTIES CXX_STANDARD 17)
    if (PLD_WINDOWS)
        target_compile_definitions(${TGT} PRIVATE -DPLD_WINDOWS=1 -DPLD_TC_VC=1)
    elseif (PLD_LINUX)
        target_compile_definitions(${TGT} PRIVATE -DPLD_LINUX=1 -DPLD_TC_GCC=1)
        target_compile_options(${TGT} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    endif ()
endfunction()

# flags required to be ABI compatible with PRTX (the codecs code),
# see https://github.com/esri/cityengine-sdk#general-software-requirements
function(pld_set_prtx_compiler_flags TGT)
    if (PLD_WINDOWS)
        target_compile_options(${TGT} PRIVATE -bigobj -GR -EHsc $<IF:$<CONFIG:Debug>,-MDd,-MD>)
    elseif (PLD_LINUX)
        target_compile_options(${TGT} PRIVATE -march=nocona -D_GLIBCXX_USE_CXX11_ABI=0)
    endif ()
endfunction()