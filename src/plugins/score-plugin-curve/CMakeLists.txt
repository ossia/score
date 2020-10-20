project(score_plugin_curve LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/CurveCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/MovePoint.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurvePresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveStyle.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveConversion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/CreatePointCommandObject.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/MovePointCommandObject.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/MoveSegmentCommandObject.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/PenCommandObject.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/SetSegmentParametersCommandObject.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurveEditionSettings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePaletteBaseEvents.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePaletteBaseStates.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePaletteBaseTransitions.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePalette.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePoint.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/OngoingState.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/States/SelectionState.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/CurveTool.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/MoveTool.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/SmartTool.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Process/CurveProcessFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Process/CurveProcessModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Process/CurveProcessPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentData.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentList.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModelSerialization.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Linear/LinearSegment.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/PointArray/PointArraySegment.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/PointArray/psimpl.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerSegment.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/EasingSegment.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsView.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Curve/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_curve.hpp"

  )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Process/CurveProcessModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/SetSegmentParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/UpdateCurve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/CurveCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Commands/MovePoint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Point/CurvePointView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/CurveSegmentView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Linear/LinearSegment.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/Power/PowerSegment.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Segment/PointArray/PointArraySegment.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/MoveSegmentCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/CreatePointCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/MovePointCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/CurveCommandObjectBase.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/SetSegmentParametersCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CommandObjects/PenCommandObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/CurveTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/MoveTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/Tools/SmartTool.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurvePalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Palette/CurveEditionSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/Settings/CurveSettingsView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurvePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Curve/CurveView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Curve/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_curve.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
                     score_lib_base score_lib_process score_lib_state)

setup_score_plugin(${PROJECT_NAME})
