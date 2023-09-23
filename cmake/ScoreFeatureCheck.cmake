if(NOT SCORE_DEPLOYMENT_BUILD)
  return()
endif()

# General checks
set(SCORE_MISSING_PLUGINS)
set(SCORE_MISSING_FEATURES)

function(score_assert_plugin name)
  if("${name}" IN_LIST "${SCORE_DISABLED_PLUGINS}")
    return()
  endif()
  if(NOT "${name}" IN_LIST "${SCORE_PLUGINS_LIST}")
    list(APPEND SCORE_MISSING_PLUGINS "${name}")
  endif()
endfunction()

function(score_assert_feature name)
  if(NOT "${name}" IN_LIST "${SCORE_FEATURES_LIST}")
    list(APPEND SCORE_MISSING_FEATURES "${name}")
  endif()
endfunction()

score_assert_plugin(score-plugin-gfx)
score_assert_plugin(score-plugin-media)
score_assert_plugin(score-plugin-avnd)
score_assert_plugin(score-plugin-midi)
score_assert_feature(analysis_kfr)
score_assert_feature(sdl)

if(NOT EMSCRIPTEN)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
      score_assert_plugin(score-plugin-jit)
    endif()

    score_assert_plugin(score-plugin-faust)
    score_assert_plugin(score-plugin-js)
    score_assert_plugin(score-plugin-pd)
    score_assert_plugin(score-plugin-vst)
    score_assert_plugin(score-plugin-vst3)
    score_assert_plugin(score-plugin-ysfx)

    score_assert_feature(portaudio)
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
    score_assert_feature(v4l2)
endif()

if(EMSCRIPTEN)
endif()

if(SCORE_MISSING_PLUGINS)
  message(FATAL_ERROR "Deployment build is missing the following plug-ins: ${SCORE_MISSING_PLUGINS}")
endif()
if(SCORE_MISSING_FEATURES)
    message(FATAL_ERROR "Deployment build is missing the following features: ${SCORE_MISSING_FEATURES}")
endif()
