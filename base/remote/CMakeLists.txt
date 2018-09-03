cmake_minimum_required(VERSION 3.0)
project(score_remote LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRCS
"${SCORE_ROOT_SOURCE_DIR}/base/lib/score/serialization/StringConstants.cpp"

"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-device/Device/Address/AddressSettings.cpp"
"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-device/Device/Node/DeviceNode.cpp"

"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-state/State/Domain.cpp"
"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-state/State/Value.cpp"
"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-state/State/ValueConversion.cpp"
"${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-state/State/Address.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Models/CentralItemModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/NodeModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetListModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetAddressSetup.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/GUIItem.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WidgetKind.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ZeroconfListener.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/CentralItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/NodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/WidgetListModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Models/GUIItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ZeroconfListener.cpp"

)

# These two lines are used to display the QMLs in the project view of IDEs
file(GLOB_RECURSE QMLS "qml/*.qml")
add_custom_target(score_remote_ui_qml SOURCES ${QMLS})

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc")
add_executable(score_remote_ui WIN32
    ${SRCS} ${QRCS})

target_link_libraries(score_remote_ui PRIVATE ossia Qt5::Core Qt5::Widgets Qt5::Qml Servus_iface)
target_include_directories(score_remote_ui PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/base/lib"
    "${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-state"
    "${SCORE_ROOT_SOURCE_DIR}/base/plugins/score-lib-device"
    "${SCORE_ROOT_BINARY_DIR}/base/lib"
    "${SCORE_ROOT_BINARY_DIR}/base/plugins/score-lib-state"
    "${SCORE_ROOT_BINARY_DIR}/base/plugins/score-lib-device"
    )
setup_score_common_exe_features(score_remote_ui)


if(DEPLOYMENT_BUILD)
  set_target_properties(score_remote_ui PROPERTIES MACOSX_BUNDLE TRUE)
endif()

install(
  TARGETS ${APPNAME}
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)
