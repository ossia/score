cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_curve)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Curve/*.hpp"
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

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Process/CurveProcessModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/CurveCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/MovePoint.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/StateMachine/CurveEditionSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurvePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_curve.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

if(TARGET iscore_lib_inspector)
    target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})
