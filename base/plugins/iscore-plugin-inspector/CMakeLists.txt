cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_inspector LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Inspector interface
file(GLOB_RECURSE INTERFACE_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp")

set(INTERFACE_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/Separator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetBase.cpp"
)

add_library(iscore_lib_inspector ${INTERFACE_HDRS} ${INTERFACE_SRCS})
target_link_libraries(iscore_lib_inspector PUBLIC Qt5::Core Qt5::Widgets iscore_lib_base)

setup_iscore_library(iscore_lib_inspector)


# Inspector panel plug-in
file(GLOB_RECURSE PLUGIN_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/*.hpp")
set(PLUGIN_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/iscore_plugin_inspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/SelectionStackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelView.cpp"
)

add_library(iscore_plugin_inspector ${PLUGIN_HDRS} ${PLUGIN_SRCS})
target_link_libraries(iscore_plugin_inspector PRIVATE iscore_lib_inspector)

setup_iscore_plugin(iscore_plugin_inspector)
