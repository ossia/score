cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_curve)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

# Files & main target
include_directories("${SCENARIO_PROCESS_INTERFACE_FOLDER}")
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/LinearCurveSegmentModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/GammaCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/SinCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/ChangeAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/GammaCurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/LinearCurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/LinearCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/SinCurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/GammaCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/SinCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CommandObjects/MoveSegmentCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CommandObjects/CreatePointCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CommandObjects/MovePointCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CommandObjects/CurveCommandObjectBase.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/States/Tools/CurveTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/States/Tools/MoveTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/States/Tools/SelectionTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CurveStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurvePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/CurvePlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationStateInspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationStateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/AutomationState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/AutomationControl.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_lib_inspector iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

cotire(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
