cmake_minimum_required(VERSION 3.2)

project(prt4houdini)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)

add_subdirectory(codec)
add_subdirectory(client)

add_dependencies(prt4houdini prt4houdini_codec)

add_custom_target(install_${PROJECT_NAME}
    make install
    DEPENDS ${PROJECT_NAME}
    COMMENT "Installing ${PROJECT_NAME}"
)
