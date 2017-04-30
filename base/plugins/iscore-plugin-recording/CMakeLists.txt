cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_recording LANGUAGES CXX)

if(NOT TARGET iscore_plugin_engine)
    return()
endif()

iscore_common_setup()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_recording.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/Record.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/EditMessages.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Inspector/RecordedMessagesInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Inspector/RecordedMessagesInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcess.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessModel.hpp"
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

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/EditMessages.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Inspector/RecordedMessagesInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordProviderFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordMessagesManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationCreationVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordAutomations/RecordAutomationFirstParameterCallbackVisitor.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_recording.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_engine)

setup_iscore_plugin(${PROJECT_NAME})
