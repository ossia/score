cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_inspector)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets)

# Inspector interface
file(GLOB_RECURSE INTERFACE_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp")

set(INTERFACE_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/Separator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetBase.cpp"
)

add_library(iscore_lib_inspector ${INTERFACE_HDRS} ${INTERFACE_SRCS})
target_link_libraries(iscore_lib_inspector Qt5::Core Qt5::Widgets iscore_lib_base)

set_property(TARGET iscore_lib_inspector APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

cotire(iscore_lib_inspector)
set_target_properties(iscore_lib_inspector
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")

install(TARGETS iscore_lib_inspector
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib/)

# Inspector panel plug-in
file(GLOB_RECURSE PLUGIN_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp")
set(PLUGIN_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/InspectorControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/InspectorPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/Implementation/SelectionStackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/Implementation/InspectorPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/InspectorPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/InspectorPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/InspectorPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/InspectorPanelView.cpp"
)

add_library(iscore_plugin_inspector ${PLUGIN_HDRS} ${PLUGIN_SRCS})

target_link_libraries(iscore_plugin_inspector iscore_lib_inspector)

cotire(iscore_plugin_inspector)
set_target_properties(iscore_plugin_inspector
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")


install(TARGETS iscore_plugin_inspector
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
