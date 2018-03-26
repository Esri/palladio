### setup conan

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
   message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
   file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.10/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP CMAKE_TARGETS BUILD missing)


### PRT dependency

find_package(prt CONFIG REQUIRED)

function(pld_add_dependency_prt TGT)
	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LIBRARY})
endfunction()


### HOUDINI dependency

list(APPEND CMAKE_PREFIX_PATH "${CONAN_HOUDINI_ROOT}/toolkit/cmake")
find_package(Houdini REQUIRED)

function(pld_add_dependency_houdini TGT)
	target_link_libraries(${TGT} PRIVATE Houdini)
	houdini_configure_target(${TGT})
endfunction()


### boost dependency

function(pld_add_dependency_boost TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::boost)
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
