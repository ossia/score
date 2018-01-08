cmake_minimum_required(VERSION 3.0)
project(score_plugin_fx LANGUAGES CXX)

add_library(
  score_plugin_fx
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/TestNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiUtil.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/AngleNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/VelToNote.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/LFO.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Chord.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Metro.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Envelope.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Quantifier.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathGenerator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathMapping.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.cpp"
)

target_link_libraries(score_plugin_fx PUBLIC score_plugin_engine score_plugin_media)
setup_score_plugin(score_plugin_fx)
