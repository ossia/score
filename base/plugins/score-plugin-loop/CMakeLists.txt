cmake_minimum_required(VERSION 3.1)
project(score_plugin_loop LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_loop.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/LoopCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/LoopTriggerCommands.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/MoveLoopEvent.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/PutInLoop.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopTriggerCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopDisplayedElements.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopViewUpdater.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Loop/Palette/LoopToolPalette.hpp"
)
set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopDisplayedElements.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/LoopViewUpdater.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Palette/LoopToolPalette.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/PutInLoop.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Commands/LoopCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopTriggerCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Loop/Inspector/LoopInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_loop.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets
                     score_lib_base score_lib_process score_plugin_scenario
                     score_lib_inspector
                     )

setup_score_plugin(${PROJECT_NAME})
