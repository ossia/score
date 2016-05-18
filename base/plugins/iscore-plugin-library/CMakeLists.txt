cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_library LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_library.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/JSONLibrary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.hpp"
)
set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/JSONLibrary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryElement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.cpp"
)

add_library(iscore_plugin_library ${SRCS} ${HDRS})

target_link_libraries(iscore_plugin_library
  PUBLIC
      Qt5::Core Qt5::Widgets
      iscore_lib_base iscore_lib_state iscore_plugin_deviceexplorer)

setup_iscore_plugin(iscore_plugin_library)
