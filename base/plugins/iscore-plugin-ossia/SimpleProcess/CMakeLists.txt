cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_ossia_simpleprocess)

if(DEPLOYMENT_BUILD)
return()
endif()

iscore_common_setup()

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia_simpleprocess.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcess.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessFactory.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia_simpleprocess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessFactory.cpp"
)


add_library(iscore_plugin_ossia_simpleprocess ${HDRS} ${SRCS})
target_link_libraries(iscore_plugin_ossia_simpleprocess
    Qt5::Core Qt5::Widgets iscore_plugin_ossia iscore_lib_dummyprocess)

setup_iscore_plugin(iscore_plugin_ossia_simpleprocess)
