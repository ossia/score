project(score_base LANGUAGES CXX)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(SCORE_FHS_BUILD)
  set(SCORE_BIN_INSTALL_DIR bin)
else()
  set(SCORE_BIN_INSTALL_DIR .)
endif()

add_subdirectory(lib)
add_subdirectory(plugins)

if(NOT SCORE_DISABLE_ADDONS)
  add_subdirectory(addons)
endif()

if(APPLE OR WIN32 OR TARGET X11::X11)
  if(TARGET score_plugin_vst)
    add_subdirectory(vstpuppet)
  endif()

  if(TARGET score_plugin_vst3)
    add_subdirectory(vst3puppet)
  endif()
endif()

if(LINUX)
  add_subdirectory(linuxcheck)
endif()

add_subdirectory(app)


if(SCORE_PLAYER)
  add_subdirectory(player)
endif()
