cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_recording LANGUAGES CXX)

if(NOT TARGET iscore_plugin_engine)
    return()
endif()

iscore_common_setup()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/RecordedMessagesProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/EditMessages.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Inspector/RecordedMessagesInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/RecordedMessages/Inspector/RecordedMessagesInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordMessagesManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_recording.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_engine)

setup_iscore_plugin(${PROJECT_NAME})
