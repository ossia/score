project(score_plugin_fx LANGUAGES CXX)

if(NOT TARGET score_plugin_avnd)
  message("WARNING: score_plugin_avnd not found, skipping ${PROJECT_NAME}")
  return()
endif()

set(HDRS

  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/AngleNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Arpeggiator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Arraygen.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Arraymap.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Chord.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/ClassicalBeat.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/DebugFx.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Envelope.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathAudioFilter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathAudioGenerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathGenerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathHelpers.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathMapping_generic.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MathValueFilter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MicroMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiUtil.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/MidiHiRes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/NoiseFilter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/FactorOracle.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/LFO.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/LFO_v2.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Metro.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/EmptyMapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Looper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Smooth.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Smooth_v2.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/TestNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Types.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/Quantifier.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/RateLimiter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Fx/VelToNote.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.hpp"
 )
add_library(
  score_plugin_fx
    ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_fx.cpp"
)

target_include_directories(score_plugin_fx PRIVATE "${OSSIA_3RDPARTY_FOLDER}")
target_link_libraries(score_plugin_fx PUBLIC score_plugin_engine score_plugin_avnd)

setup_score_plugin(score_plugin_fx)

if(MSVC)
    # D:\a\score\score\src\plugins\score-plugin-fx\Fx\MidiUtil.hpp(145,77): error C2131: expression did not evaluate to a constant
    #       C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.41.34120\include\xutility(1817,9):
    #       failure was caused by evaluation exceeding step limit of 1048576 (/constexpr:steps<NUMBER>)
    target_compile_options(score_plugin_fx PRIVATE /constexpr:steps16000000)
endif()
if(WIN32 AND NOT MSVC AND NOT SCORE_PCH)
  target_compile_options(score_plugin_fx PRIVATE -Ofast -fno-finite-math-only -Wa,-mbig-obj)
endif()

if(0)#BUILD_TESTING AND NOT SCORE_DYNAMIC_PLUGINS)
  if(NOT TARGET Catch2::Catch2WithMain)
    include(CTest)
    set(CATCH_BUILD_STATIC_LIBRARY 1)
    add_subdirectory("${OSSIA_3RDPARTY_FOLDER}/Catch2" Catch2)
  endif()

  if(TARGET Catch2::Catch2WithMain)
    ossia_add_test(FactorOracleMIDITest Tests/FactorOracleMIDITest.cpp)
    target_link_libraries(ossia_FactorOracleMIDITest PRIVATE score_plugin_engine)
    setup_score_common_test_features(ossia_FactorOracleMIDITest)
  endif()
endif()
