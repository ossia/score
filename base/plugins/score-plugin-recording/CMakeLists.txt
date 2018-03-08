cmake_minimum_required(VERSION 3.1)
project(score_plugin_recording LANGUAGES CXX)

if(NOT TARGET score_plugin_engine)
    return()
endif()

score_common_setup()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_recording.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/Record.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationCreationVisitor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationFirstParameterCallbackVisitor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationParameterCallbackVisitor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordData.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordManager.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordMessagesManager.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordProviderFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordProviderFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordMessagesManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationCreationVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationFirstParameterCallbackVisitor.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_recording.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_moc_headers(${PROJECT_NAME} "${HDRS}")
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_engine)

setup_score_plugin(${PROJECT_NAME})
