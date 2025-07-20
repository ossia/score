if(CMAKE_SYSTEM_NAME MATCHES Emscripten)
  set(SCORE_PLUGINS_TO_BUILD

  score-lib-inspector
  score-lib-state
  score-lib-device
  score-plugin-deviceexplorer
  score-lib-localtree
  score-lib-process

  score-plugin-library
  score-plugin-transport

  score-plugin-inspector

  score-plugin-curve
  score-plugin-automation
  score-plugin-scenario

  score-plugin-protocols

  score-plugin-audio
  score-plugin-engine

  score-plugin-dataflow

  score-plugin-media

#  score-plugin-gfx
  score-plugin-js
  score-plugin-midi
  score-plugin-recording

  score-plugin-nodal
  score-plugin-controlsurface
  score-plugin-remotecontrol
  score-plugin-spline


  score-plugin-avnd

  score-plugin-fx
  score-plugin-ui
  score-plugin-analysis
)
else()
  set(SCORE_PLUGINS_TO_BUILD

  score-lib-inspector
  score-lib-state
  score-lib-device
  score-plugin-deviceexplorer
  score-lib-localtree
  score-lib-process

  score-plugin-library
  score-plugin-transport

  score-plugin-inspector


  score-plugin-curve
  score-plugin-mapping
  score-plugin-automation
  score-plugin-scenario

  score-plugin-protocols

  score-plugin-audio
  score-plugin-engine

  score-plugin-dataflow

  score-plugin-media
  score-plugin-gfx

  score-plugin-js


  score-plugin-midi
  score-plugin-recording

  score-plugin-nodal
  score-plugin-controlsurface
  score-plugin-remotecontrol
  score-plugin-spline


  score-plugin-packagemanager

  score-plugin-avnd
  score-plugin-jit
  score-plugin-spline3d
  score-plugin-vst
  score-plugin-vst3
  score-plugin-lv2
  score-plugin-clap
  score-plugin-faust
  score-plugin-pd
  score-plugin-ysfx
  score-plugin-fx
  score-plugin-ui
  score-plugin-analysis
  score-plugin-threedim
  )
endif()
