cmake_minimum_required(VERSION 3.13)


### target definition

add_library(${TGT_FS} SHARED
        main.cpp
        RulePackageFS.cpp
        ../palladio/Utils.cpp
        ../palladio/LogHandler.cpp)

target_include_directories(${TGT_FS} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

### compiler settings

pld_set_common_compiler_flags(${TGT_FS})
pld_add_version_definitions(${TGT_FS})

if (PLD_LINUX)
    set_target_properties(${TGT_FS} PROPERTIES
            INSTALL_RPATH "\$ORIGIN/../../packages/palladio"
            INSTALL_RPATH_USE_LINK_PATH FALSE
            SKIP_RPATH FALSE
            BUILD_WITH_INSTALL_RPATH TRUE)
endif ()


### dependencies

pld_add_dependency_prt(${TGT_FS})
pld_add_dependency_houdini(${TGT_FS})


### setup install target

install(TARGETS ${TGT_FS}
        RUNTIME DESTINATION ${HOUDINI_RELATIVE_DSO_PATH}/fs
        LIBRARY DESTINATION ${HOUDINI_RELATIVE_DSO_PATH}/fs)
