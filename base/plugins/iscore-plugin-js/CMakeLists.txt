cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_js)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Qml)
if(NOT TARGET iscore_plugin_ossia)
  return()
endif()

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSAPIWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessMetadata.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/EditScript.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/JSCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/Inspector/JSInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Inspector/JSInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_js.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::Qml
                     iscore_lib_base iscore_plugin_ossia iscore_lib_dummyprocess)

setup_iscore_plugin(${PROJECT_NAME})
