cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_curve)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

# Files & main target
include_directories("${SCENARIO_PROCESS_INTERFACE_FOLDER}")
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
set(CURVE_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Automation/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Commands/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Curve/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/AutomationControl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CurvePlugin.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/LinearCurveSegmentModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/GammaCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/SinCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/ChangeAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/LinearCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/GammaCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerCurveSegmentModel.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_curve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/AutomationState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/AutomationControl.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationStateInspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationStateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/AutomationInspectorWidget.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

if(TARGET iscore_lib_inspector)
    # TODO instead put the definition in the INTERFACE of iscore_lib_inspector
    target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
endif()

iscore_cotire(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)
