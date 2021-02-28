project(score_base LANGUAGES CXX)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(lib)
add_subdirectory(plugins)

if(SCORE_EMBEDDED)
  return()
endif()
if(NOT SCORE_DISABLE_ADDONS)
  add_subdirectory(addons)
endif()

if(TARGET score_plugin_vst)
  add_subdirectory(vstpuppet)
endif()

if(TARGET score_plugin_vst3)
  add_subdirectory(vst3puppet)
endif()

add_subdirectory(app)

if(SCORE_PLAYER)
  add_subdirectory(player)
endif()

if(SCORE_FX_DESIGNER)
  add_subdirectory(fxdesigner)
endif()
