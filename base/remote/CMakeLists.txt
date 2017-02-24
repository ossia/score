cmake_minimum_required(VERSION 3.0)
project(iscore_remote LANGUAGES CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Models/CentralItemModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/NodeModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetListModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetAddressSetup.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/GUIItem.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WebSocketClient.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WidgetKind.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/CentralItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/NodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetListModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/GUIItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WebSocketClient.cpp"

)

# These two lines are used to display the QMLs in the project view of IDEs
file(GLOB_RECURSE QMLS "qml/*.qml")
add_custom_target(iscore_remote_ui_qml SOURCES ${QMLS})

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc")
add_executable(iscore_remote_ui ${SRCS} ${QRCS})

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  # TODO Investigate why this is necessary ? (see testapp)
  target_link_libraries(iscore_remote_ui PUBLIC iscore_lib_base iscore_lib_device iscore_plugin_engine Qt5::Core Qt5::Widgets Qt5::WebSockets Qt5::Qml)
else()
  target_link_libraries(iscore_remote_ui PRIVATE iscore_lib_base iscore_lib_device iscore_plugin_engine Qt5::Core Qt5::Widgets Qt5::WebSockets Qt5::Qml)
endif()
set(ISCORE_COTIRE False)
setup_iscore_common_exe_features(iscore_remote_ui)
