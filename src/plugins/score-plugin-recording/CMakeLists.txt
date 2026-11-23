project(score_plugin_recording LANGUAGES CXX)

if(NOT TARGET score_plugin_engine)
    return()
endif()

score_common_setup()
# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_recording.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/WavStreamWriter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/AudioRecorder.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/MidiRecorder.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/PlaceTakes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Session/RecordOptions.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Session/RecordSession.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Session/RecordPortMenu.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/ValueRecorder.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedValues.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedCurves.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/PlaceAddressTakes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/OverdubNotes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/Passes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Recording/Settings/RecordingSettings.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Commands/RecordingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Record/RecordTools.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/WavStreamWriter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/AudioRecorder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/MidiRecorder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedNotes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Session/RecordSession.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Session/RecordPortMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Engine/ValueRecorder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedValues.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/RecordedCurves.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/PlaceAddressTakes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/OverdubNotes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Takes/Passes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Recording/Settings/RecordingSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Recording/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_recording.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_scenario score_plugin_engine score_plugin_curve score_plugin_automation score_plugin_midi score_plugin_media)

setup_score_plugin(${PROJECT_NAME})
