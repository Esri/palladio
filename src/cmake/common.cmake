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


### versioning tools

function(pld_add_version_definitions TGT)
    # we use semver.org, Semantic Versioning 2.0.0
    # i.e. <major>.<minor>.<patch>+b<buildnr>.hdn<houdini ver>.prt<prt ver>
    set(PLD_VERSION "${PLD_VERSION_MAJOR}.${PLD_VERSION_MINOR}.${PLD_VERSION_PATCH}${PLD_VERSION_PRE}\
+b${PLD_VERSION_BUILD}\
.hdn${Houdini_VERSION_MAJOR}-${Houdini_VERSION_MINOR}-${Houdini_VERSION_PATCH}\
.prt${PRT_VERSION_MAJOR}-${PRT_VERSION_MINOR}-${PRT_VERSION_MICRO}")
    message(STATUS "For ${TGT}, use PLD_VERSION = ${PLD_VERSION}")

    target_compile_definitions(${TGT} PRIVATE
            -DPLD_VERSION_MAJOR=${PLD_VERSION_MAJOR}
            -DPLD_VERSION_MINOR=${PLD_VERSION_MINOR}
            -DPLD_VERSION_PATCH=${PLD_VERSION_PATCH}
            -DPLD_VERSION_PRE=\"${PLD_VERSION_PRE}\" # quoted, might be a string
            -DPLD_VERSION_BUILD=${PLD_VERSION_BUILD}
            -DPLD_VERSION=\"${PLD_VERSION}\") # quoted, is a string
endfunction()
