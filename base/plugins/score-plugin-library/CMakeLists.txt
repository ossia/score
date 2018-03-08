cmake_minimum_required(VERSION 3.0)
project(score_plugin_library LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_library.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/JSONLibrary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.hpp"
)
set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/JSONLibrary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryElement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/JSONLibrary/LibraryWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.cpp"
)

add_library(score_plugin_library ${SRCS} ${HDRS})

score_moc_headers(${PROJECT_NAME} "${HDRS}")
target_link_libraries(score_plugin_library
  PUBLIC
      Qt5::Core Qt5::Widgets
      score_lib_base score_lib_state score_plugin_deviceexplorer score_lib_process)

setup_score_plugin(score_plugin_library)
