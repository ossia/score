cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_automation LANGUAGES CXX)

iscore_common_setup()

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

  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/AutomationCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/ChangeAddress.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/InitAutomation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/SetAutomationMax.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/SetAutomationMin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/CurvePointInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Automation/State/AutomationState.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_automation.hpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_automation.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME}
  PUBLIC Qt5::Core Qt5::Widgets
            iscore_lib_base iscore_lib_device
            iscore_plugin_deviceexplorer iscore_lib_process
            iscore_lib_state iscore_plugin_curve iscore_lib_inspector
            tinyspline)

setup_iscore_plugin(${PROJECT_NAME})
