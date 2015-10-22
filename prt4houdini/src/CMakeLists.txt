cmake_minimum_required(VERSION 3.2)

project(prt4houdini)

set(P4H_DEP_CACHE ${PROJECT_BINARY_DIR}/../depcache)

add_subdirectory(codec)
add_subdirectory(client)