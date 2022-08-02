cmake_minimum_required(VERSION 3.13)

function(pld_setup_packaging)
    set(CPACK_PACKAGE_NAME "palladio")
    set(CPACK_PACKAGE_VENDOR "Esri R&D Zurich / VRBN")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PLD_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PLD_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PLD_VERSION_PATCH}${PLD_VERSION_PRE})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "palladio-${PLD_VERSION}")

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(PLD_PKG_OS "windows")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PLD_PKG_OS "linux")
    endif ()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PLD_VERSION}-${PLD_PKG_OS}")

    if (PLD_WINDOWS)
        set(CPACK_GENERATOR ZIP)
    else ()
        set(CPACK_GENERATOR TGZ)
    endif ()

    include(CPack)

    # not strictly necessary, useful for IDEs like CLion without direct support to run "make package"
    add_custom_target(${TGT_PACKAGE}
            ${CMAKE_MAKE_PROGRAM} package
            DEPENDS ${TGT_PALLADIO}
            COMMENT "Packaging Palladio")
endfunction()
