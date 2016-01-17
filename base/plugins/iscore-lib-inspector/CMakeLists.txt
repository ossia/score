cmake_minimum_required(VERSION 3.0)
project(iscore_lib_inspector LANGUAGES CXX)

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

add_library(${PROJECT_NAME} ${INTERFACE_HDRS} ${INTERFACE_SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core Qt5::Widgets iscore_lib_base)

setup_iscore_library(iscore_lib_inspector)
