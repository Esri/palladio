### setup conan

set(PLD_CONAN_TOOLS "${CMAKE_SOURCE_DIR}/../conan")

if(PLD_WINDOWS)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v141")
elseif(PLD_LINUX)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc63")
endif()

if(PLD_CONAN_CESDK_DIR)
	# make conan ignore cesdk package, we'll set its path manually
	set(PLD_CONAN_ENV "PLD_CONAN_SKIP_CESDK=1")
	set(prt_DIR "${PLD_CONAN_CESDK_DIR}/cmake")
	message(STATUS "Ignoring conan package for cesdk, using local path: ${prt_DIR}")
endif()

include(${PLD_CONAN_TOOLS}/conan-0.13.cmake)
conan_cmake_run(CONANFILE conanfile.py PROFILE ${PLD_CONAN_PROFILE} BASIC_SETUP CMAKE_TARGETS ENV ${PLD_CONAN_ENV})


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
	# houdini_configure_target(${TGT})
	# note: the above disabled function (provided by sidefx) does weird things with the install prefix
endfunction()


### boost dependency

function(pld_add_dependency_boost TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::boost)
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
