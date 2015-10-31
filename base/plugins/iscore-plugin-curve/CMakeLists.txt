cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_curve)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

# Files & main target
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
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Linear/LinearCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Gamma/GammaCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Sin/SinCurveSegmentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/PointArray/PointArrayCurveSegmentSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationLayerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationLayerModel.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingLayerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingLayerModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Commands/InitAutomation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/ChangeAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Linear/LinearCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Gamma/GammaCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Sin/SinCurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/PointArray/PointArrayCurveSegmentModel.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveStyle.cpp"

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

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Curve/resources/AutomationResources.qrc")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

if(TARGET iscore_lib_inspector)
    target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})
