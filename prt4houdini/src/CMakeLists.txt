cmake_minimum_required(VERSION 3.8)

# versioning follows semver.org, Semantic Versioning 2.0.0
# i.e. <major>.<minor>.<patch>+b<buildnr>
set(P4H_VERSION_MAJOR 0)
set(P4H_VERSION_MINOR 7)
set(P4H_VERSION_PATCH 1)
if(NOT P4H_VERSION_BUILD)
    set(P4H_VERSION_BUILD DEV)
endif()
set(P4H_VERSION "${P4H_VERSION_MAJOR}.${P4H_VERSION_MINOR}.${P4H_VERSION_PATCH}+b${P4H_VERSION_BUILD}")

# TODO: include houdini/prt version into semver metadata? eg. append ".h16.0.705.ce1.9.1234"


### root project

project(prt4houdini)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)

add_subdirectory(codec)
add_subdirectory(client)
add_subdirectory(test)

add_dependencies(prt4houdini prt4houdini_codec)

add_custom_target(install_${PROJECT_NAME}
    make install
    DEPENDS ${PROJECT_NAME}
    COMMENT "Installing ${PROJECT_NAME}")


### package targets

set(CPACK_PACKAGE_NAME                "prt4houdini")
set(CPACK_PACKAGE_VENDOR              "buehler/haegler")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
set(CPACK_PACKAGE_VERSION_MAJOR       ${P4H_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${P4H_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${P4H_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "prt4houdini-${P4H_VERSION}")
set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${P4H_VERSION}-${P4H_OS}-h${HOUDINI_VERSION}")

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
