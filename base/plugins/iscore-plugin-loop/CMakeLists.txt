cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_loop)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Qml)

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopLayer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopViewUpdater.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Palette/LoopToolPalette.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/LoopCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_loop.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_plugin_scenario iscore_lib_dummyprocess)

setup_iscore_plugin(${PROJECT_NAME})
