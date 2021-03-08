project(score_plugin_midi LANGUAGES CXX)

if(NOT OSSIA_PROTOCOL_MIDI)
  return()
endif()

# Files & main target
set(HEADER_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/AddNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/RemoveNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/MoveNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/ScaleNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/SetOutput.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/CommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Inspector/MidiProcessInspector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcess.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiDrop.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNoteView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiExecutor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiStyle.hpp"

  Patternist/PatternModel.hpp
  Patternist/PatternView.hpp
  Patternist/PatternPresenter.hpp
  Patternist/PatternFactory.hpp
  Patternist/PatternExecutor.hpp
  Patternist/PatternMetadata.hpp
  Patternist/PatternCommandFactory.hpp
  Patternist/PatternInspector.hpp
  Patternist/Commands/PatternProperties.hpp

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_midi.hpp"
  )

set(SOURCE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/AddNote.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/RemoveNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/MoveNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/ScaleNotes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/SetOutput.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Commands/CommandFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/Inspector/MidiProcessInspector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiProcess.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiPresenter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiDrop.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNoteView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiNote.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Midi/MidiExecutor.cpp"

  Patternist/PatternModel.cpp
  Patternist/PatternView.cpp
  Patternist/PatternPresenter.cpp
  Patternist/PatternExecutor.cpp

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_midi.cpp"
  )

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
score_generate_command_list_file(${PROJECT_NAME} "${HEADER_FILES}")
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_engine libremidi)
target_include_directories(${PROJECT_NAME} PRIVATE "${OSSIA_3RDPARTY_FOLDER}/GSL")

setup_score_plugin(${PROJECT_NAME})
