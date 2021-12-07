project(score_plugin_fx LANGUAGES CXX)

set(HDRS

  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/AngleNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Arpeggiator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/ClassicalBeat.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/TestNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiUtil.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiHiRes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/AngleNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/VelToNote.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/FactorOracle.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/LFO.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Chord.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Gain.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Metro.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Envelope.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Quantifier.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/EmptyMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathGenerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Looper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/DebugFx.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/ValueFilter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/RateLimiter.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.hpp"
 )
add_library(
  score_plugin_fx
    ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.cpp"
)

target_link_libraries(score_plugin_fx PUBLIC score_plugin_engine)

setup_score_plugin(score_plugin_fx)

if(WIN32 AND NOT MSVC AND NOT SCORE_PCH)
  target_compile_options(score_plugin_fx PRIVATE -Ofast -fno-finite-math-only -Wa,-mbig-obj)
endif()

if(BUILD_TESTING AND NOT SCORE_DYNAMIC_PLUGINS)
  if(NOT TARGET Catch2::Catch2WithMain)
    include(CTest)
    add_subdirectory("${OSSIA_3RDPARTY_FOLDER}/Catch2" Catch2)
  endif()
  ossia_add_test(FactorOracleMIDITest Tests/FactorOracleMIDITest.cpp)
  target_link_libraries(ossia_FactorOracleMIDITest PRIVATE score_plugin_engine)
  setup_score_common_test_features(ossia_FactorOracleMIDITest)
endif()
