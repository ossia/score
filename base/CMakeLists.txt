project(score_base LANGUAGES CXX)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(lib)
add_subdirectory(plugins)

if(NOT SCORE_DISABLE_ADDONS)
  add_subdirectory(addons)
endif()

if(TARGET score_plugin_media)
  add_subdirectory(vstpuppet)
endif()

add_subdirectory(app)

if(SCORE_PLAYER)
  add_subdirectory(player)
endif()

if(SCORE_COVERAGE)
    add_subdirectory(app-test)
endif()
