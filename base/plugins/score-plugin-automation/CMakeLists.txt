cmake_minimum_required(VERSION 3.1)
project(score_plugin_automation LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationColors.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomMetadata.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomMetadata.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeColors.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomePresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeProcessMetadata.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/AutomationCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/ChangeAddress.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/InitAutomation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/SetAutomationMax.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/State/AutomationState.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_automation.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Color/GradientAutomView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Spline/SplineAutomView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Metronome/MetronomeView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/InitAutomation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/ChangeAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/AutomationCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/State/AutomationState.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_automation.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME}
  PUBLIC Qt5::Core Qt5::Widgets
            score_lib_base score_lib_device
            score_plugin_deviceexplorer score_lib_process
            score_lib_state score_plugin_curve score_lib_inspector
            )

setup_score_plugin(${PROJECT_NAME})
