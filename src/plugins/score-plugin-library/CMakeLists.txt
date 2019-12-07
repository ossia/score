project(score_plugin_library LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_library.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibraryWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/ProcessesItemModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/FileSystemModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibrarySettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibraryInterface.hpp"
)
set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibraryInterface.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelDelegate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/Panel/LibraryPanelFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibraryWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Library/LibrarySettings.cpp"
)

add_library(score_plugin_library ${SRCS} ${HDRS})

target_link_libraries(score_plugin_library
  PUBLIC
      Qt5::Core Qt5::Widgets
      score_lib_base score_lib_process)

setup_score_plugin(score_plugin_library)
