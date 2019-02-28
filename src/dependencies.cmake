### setup conan

set(PLD_CONAN_TOOLS "${CMAKE_SOURCE_DIR}/../conan")
include(${PLD_CONAN_TOOLS}/conan-0.13.cmake)
conan_check(VERSION 1.11 REQUIRED)


### select Houdini version and required toolchain

if (NOT PLD_HOUDINI_VERSION)
	set(PLD_HOUDINI_VERSION "17.0") # use latest Houdini 17 by default
endif()

string(REPLACE "." ";" PLD_HDN_VER "${PLD_HOUDINI_VERSION}")
list(GET PLD_HDN_VER 0 PLD_HDN_VER_MAJ)
list(GET PLD_HDN_VER 1 PLD_HDN_VER_MIN)

# Houdini 17.5
if (${PLD_HDN_VER_MAJ} STREQUAL "17" AND ${PLD_HDN_VER_MIN} STREQUAL "5")
	message(STATUS "Asking Conan for Houdini 17.5...")
	set(PLD_CONANFILE "conanfile-h175.py")
	if(PLD_WINDOWS)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v141")
	elseif(PLD_LINUX)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc63")
	endif()

# Houdini 17.0
elseif (${PLD_HDN_VER_MAJ} STREQUAL "17" AND ${PLD_HDN_VER_MIN} STREQUAL "0")
	message(STATUS "Asking Conan for Houdini 17.0...")
	set(PLD_CONANFILE "conanfile-h17.py")
	if(PLD_WINDOWS)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v141")
	elseif(PLD_LINUX)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc63")
	endif()

# Houdini 16.5
elseif (${PLD_HDN_VER_MAJ} STREQUAL "16" AND ${PLD_HDN_VER_MIN} STREQUAL "5")
	message(STATUS "Asking Conan for Houdini 16.5...")
	set(PLD_CONANFILE "conanfile-h16.py")
	if(PLD_WINDOWS)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v140")
	elseif(PLD_LINUX)
		set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc48")
	endif()
endif()

# TODO: add support to control micro version of houdini via conan env


### select required CE SDK version

if(PLD_CONAN_CESDK_DIR)
	# make conan ignore cesdk package, we'll set its path manually
	set(PLD_CONAN_ENV "PLD_CONAN_SKIP_CESDK=1")
	set(prt_DIR "${PLD_CONAN_CESDK_DIR}/cmake")
	message(STATUS "Ignoring conan package for cesdk, using local path: ${prt_DIR}")
endif()

# TODO: allow actual specification of CE SDK version via conan env


### run conan

conan_cmake_run(CONANFILE ${PLD_CONANFILE} PROFILE ${PLD_CONAN_PROFILE} BASIC_SETUP CMAKE_TARGETS ENV ${PLD_CONAN_ENV})


### PRT dependency

find_package(prt CONFIG REQUIRED)

install(FILES ${PRT_LIBRARIES} DESTINATION .)
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION prtlib)

function(pld_add_dependency_prt TGT)
	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LINK_LIBRARIES})
endfunction()


### Houdini dependency

list(APPEND CMAKE_PREFIX_PATH "${CONAN_HOUDINI_ROOT}/toolkit/cmake")
find_package(Houdini REQUIRED)

function(pld_add_dependency_houdini TGT)
	target_compile_definitions(${TGT} PRIVATE -DHOUDINI_VERSION_MAJOR=${PLD_HDN_VER_MAJ} -DHOUDINI_VERSION_MINOR=${PLD_HDN_VER_MIN})
	if(${Houdini_VERSION_MAJOR} STREQUAL "17")
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=hboost)
		if(PLD_WINDOWS)
			set(houdini_lib_path "${_houdini_install_root}/custom/houdini/dsolib")
			set(houdini_hboost_fs hboost_filesystem-mt)
		elseif(PLD_LINUX)
			set(houdini_lib_path "${_houdini_install_root}/dsolib")
			set(houdini_hboost_fs hboost_filesystem)
		endif()
		find_library(HBOOST_FILESYSTEM ${houdini_hboost_fs} ${houdini_lib_path})
		target_link_libraries(${TGT} PRIVATE Houdini ${HBOOST_FILESYSTEM})
		houdini_configure_target(${TGT})
	elseif(${Houdini_VERSION_MAJOR} STREQUAL "16")
		target_link_libraries(${TGT} PRIVATE Houdini)
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=boost)
	endif()
endfunction()


### boost dependency

function(pld_add_dependency_boost TGT)
	if(${Houdini_VERSION_MAJOR} STREQUAL "16")
		target_link_libraries(${TGT} PRIVATE CONAN_PKG::boost)
	endif()
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
