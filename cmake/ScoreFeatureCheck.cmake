if(NOT SCORE_DEPLOYMENT_BUILD)
  return()
endif()

# General checks
set(SCORE_MISSING_PLUGINS)
set(SCORE_MISSING_FEATURES)

function(score_assert_plugin name)
  # The asserts below spell plug-ins the way the directories are named, while
  # SCORE_PLUGINS_LIST holds target names.
  string(REPLACE "-" "_" target "${name}")

  if("${name}" IN_LIST SCORE_DISABLED_PLUGINS OR "${target}" IN_LIST SCORE_DISABLED_PLUGINS)
    return()
  endif()
  if(NOT "${target}" IN_LIST SCORE_PLUGINS_LIST)
    list(APPEND SCORE_MISSING_PLUGINS "${name}")
    set(SCORE_MISSING_PLUGINS "${SCORE_MISSING_PLUGINS}" PARENT_SCOPE)
  endif()
endfunction()

function(score_assert_feature name)
  if(NOT "${name}" IN_LIST SCORE_FEATURES_LIST)
    list(APPEND SCORE_MISSING_FEATURES "${name}")
    set(SCORE_MISSING_FEATURES "${SCORE_MISSING_FEATURES}" PARENT_SCOPE)
  endif()
endfunction()

score_assert_plugin(score-plugin-gfx)
score_assert_plugin(score-plugin-media)
score_assert_plugin(score-plugin-avnd)
score_assert_plugin(score-plugin-midi)
score_assert_feature(analysis_kfr)
score_assert_feature(sdl)

if(NOT EMSCRIPTEN)
    # AMD64 is the Windows spelling, x86_64 the one Linux and macOS report.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(AMD64|x86_64)")
      score_assert_plugin(score-plugin-jit)
    endif()

    score_assert_plugin(score-plugin-faust)
    score_assert_plugin(score-plugin-js)
    score_assert_plugin(score-plugin-pd)
    score_assert_plugin(score-plugin-vst)
    score_assert_plugin(score-plugin-vst3)
    score_assert_plugin(score-plugin-ysfx)

    if(NOT APPLE)
      score_assert_feature(portaudio)
    endif()
    score_assert_feature(jack)
    score_assert_feature(jack_transport)
    score_assert_feature(dnssd)
    score_assert_feature(hap)
    score_assert_feature(gpu_js)

    score_assert_feature(protocol_osc)
    score_assert_feature(protocol_minuit)
    score_assert_feature(protocol_oscquery)
    score_assert_feature(protocol_midi)
    score_assert_feature(protocol_http)
    score_assert_feature(protocol_ws)
    score_assert_feature(protocol_serial)
    score_assert_feature(protocol_joystick)
    score_assert_feature(protocol_wiimote)
    score_assert_feature(protocol_artnet)
    score_assert_feature(protocol_mapper)
endif()

if(WIN32)
    # Check for portaudio asio support
    # Check for portaudio wasapi support
    score_assert_feature(spout)
endif()

if(APPLE)
    # Check for portaudio coreaudio support
    score_assert_feature(syphon)
endif()

if(UNIX AND NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN)
    # Check for ALSA support
    # Check for PortAudio ALSA support
    score_assert_plugin(score-plugin-lv2)
    score_assert_feature(pipewire)
    score_assert_feature(shmdata)
    score_assert_feature(sh4lt)
    score_assert_feature(v4l2)
endif()

if(EMSCRIPTEN)
endif()

# Only our own release builds control the whole dependency set, so only they treat
# a gap as an error: faust, ysfx, lv2 and SDL are not packaged everywhere, and a
# distribution cannot conjure them up.
if(SCORE_MISSING_PLUGINS OR SCORE_MISSING_FEATURES)
  if(SCORE_STRICT_FEATURE_CHECK)
    set(SCORE_FEATURE_CHECK_LEVEL FATAL_ERROR)
  else()
    set(SCORE_FEATURE_CHECK_LEVEL WARNING)
  endif()

  if(SCORE_MISSING_PLUGINS)
    message(${SCORE_FEATURE_CHECK_LEVEL}
      "Deployment build is missing the following plug-ins: ${SCORE_MISSING_PLUGINS}")
  endif()
  if(SCORE_MISSING_FEATURES)
    message(${SCORE_FEATURE_CHECK_LEVEL}
      "Deployment build is missing the following features: ${SCORE_MISSING_FEATURES}")
  endif()
endif()
