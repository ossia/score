cmake_minimum_required(VERSION 3.0)
project(score_plugin_fx LANGUAGES CXX)

set(HDRS

  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/ClassicalBeat.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/TestNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiUtil.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/AngleNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/VelToNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/LFO.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Chord.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Gain.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Metro.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Envelope.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Quantifier.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/EmptyMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathGenerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathMapping.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.hpp"
 )
add_library(
  score_plugin_fx
    ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.cpp"
)

score_moc_headers(${PROJECT_NAME} "${HDRS}")
target_link_libraries(score_plugin_fx PUBLIC score_plugin_engine score_plugin_media)
if(MSVC)
  target_compile_definitions(score_plugin_fx PRIVATE INCOMPETENT_COMPILER)
endif()
setup_score_plugin(score_plugin_fx)
