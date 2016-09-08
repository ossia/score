cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_midi LANGUAGES CXX)

if(IOS)
  return()
endif()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HEADER_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/AddNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/RemoveNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/MoveNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/ScaleNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/CommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcess.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiLayer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNoteView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiExecutor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_midi.hpp"
  )

set(SOURCE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/AddNote.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/RemoveNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/MoveNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/ScaleNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/CommandFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcess.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiPresenter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNoteView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiExecutor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_midi.cpp"
  )

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
iscore_generate_command_list_file(${PROJECT_NAME} "${HEADER_FILES}")
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_engine)

setup_iscore_plugin(${PROJECT_NAME})
