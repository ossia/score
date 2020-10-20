project(score_plugin_automation LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationColors.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationExecution.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientExecution.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplinePresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineExecution.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeColors.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomePresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeExecution.hpp"

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
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/LocalTree.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_automation.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationExecution.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Color/GradientExecution.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplinePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Spline/SplineExecution.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Metronome/MetronomeExecution.cpp"

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
  PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
            score_lib_base score_lib_device
            score_plugin_deviceexplorer score_lib_process
            score_lib_state score_plugin_curve score_lib_inspector
            )

setup_score_plugin(${PROJECT_NAME})
