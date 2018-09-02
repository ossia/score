cmake_minimum_required(VERSION 3.0)
project(score_remote LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
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
"${CMAKE_CURRENT_SOURCE_DIR}/ZeroconfListener.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/CentralItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/NodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetListModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/GUIItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WebSocketClient.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ZeroconfListener.cpp"

)

# These two lines are used to display the QMLs in the project view of IDEs
file(GLOB_RECURSE QMLS "qml/*.qml")
add_custom_target(score_remote_ui_qml SOURCES ${QMLS})

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc")
add_executable(score_remote_ui ${SRCS} ${QRCS})

target_link_libraries(score_remote_ui PUBLIC ossia score_lib_device Qt5::Core Qt5::Widgets Qt5::WebSockets Qt5::Qml Servus_iface)
set(SCORE_COTIRE False)
setup_score_common_exe_features(score_remote_ui)
