cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini_codec)
project(${PROJECT})
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake)


### dependencies

prt_get_dependency("thirdparty"	"com.esri" "boost" "1.53.0esri1" "${DEP_OS}-${DEP_TC}-${P4H_ARCH}" "Boost_DIR")
find_package(Boost NO_MODULE)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})


### build target

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND SOURCES houdini.cpp encoder/HoudiniEncoder.cpp)
add_library(${PROJECT} SHARED ${SOURCES})

if(PRT_WINDOWS)
	target_link_libraries(${PROJECT} IPHlpApi Psapi DbgHelp)
elseif(PRT_LINUX)
	target_link_libraries(${PROJECT} z pthread dl rt)
endif()

target_link_libraries(${PROJECT} ${PRT_LINK_LIBRARIES})
set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)


### install target

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../install") 
install(TARGETS ${PROJECT} DESTINATION lib EXPORT "${PROJECT}Targets")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/encoder/HoudiniCallbacks.h DESTINATION include)


### install config file and export target

install(EXPORT "${PROJECT}Targets" DESTINATION "cmake")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/exportConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT}Config.cmake COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT}Config.cmake DESTINATION cmake)


