cmake_minimum_required(VERSION 3.6)

set(P4H_VERSION_MAJOR 0)
set(P4H_VERSION_MINOR 7)
set(P4H_VERSION_MICRO 0) #TODO


### root project

project(prt4houdini)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)

add_subdirectory(codec)
add_subdirectory(client)

add_dependencies(prt4houdini prt4houdini_codec)

add_custom_target(install_${PROJECT_NAME}
    make install
    DEPENDS ${PROJECT_NAME}
    COMMENT "Installing ${PROJECT_NAME}")


### package targets

set(P4H_PACKAGE_VERSION "${P4H_VERSION_MAJOR}.${P4H_VERSION_MINOR}.${P4H_VERSION_MICRO}")

set(CPACK_PACKAGE_NAME                "prt4houdini")
set(CPACK_PACKAGE_VENDOR              "buehler/haegler")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
set(CPACK_PACKAGE_VERSION_MAJOR       ${P4H_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${P4H_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${P4H_VERSION_MICRO})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "prt4houdini-${P4H_PACKAGE_VERSION}")
set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${P4H_PACKAGE_VERSION}-${P4H_OS}-h${HOUDINI_VERSION}")

if(P4H_WINDOWS)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()

include(CPack)

add_custom_target(package_${PROJECT_NAME}
    make package
    DEPENDS install_${PROJECT_NAME}
    COMMENT "Packaging ${PROJECT_NAME}")
