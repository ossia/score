cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_recordedmessages LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)
if(NOT TARGET iscore_plugin_ossia)
  return()
endif()

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/RecordedMessagesProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/RecordedMessagesProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/RecordedMessagesProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/Commands/EditMessages.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/Commands/RecordedMessagesCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/Inspector/RecordedMessagesInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecordedMessages/Inspector/RecordedMessagesInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_recordedmessages.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_plugin_ossia iscore_lib_process
                     iscore_lib_inspector iscore_lib_device iscore_plugin_deviceexplorer
                     iscore_lib_dummyprocess)

setup_iscore_plugin(${PROJECT_NAME})
