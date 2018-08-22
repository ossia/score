cmake_minimum_required(VERSION 3.0)

project(score_base LANGUAGES CXX)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(lib)

add_subdirectory(plugins)
if(NOT SCORE_DISABLE_ADDONS)
add_subdirectory(addons)
endif()

if(SCORE_MERGE_COMPONENTS)
  add_subdirectory(component-wrapper)
endif()

if(TARGET score_plugin_media)
  add_subdirectory(vstpuppet)
endif()

if(ANDROID)
#    add_subdirectory(app-android)
elseif(NACL)
    add_subdirectory(app-nacl)
else()
  add_subdirectory(app)
endif()

if(SCORE_PLAYER)
  add_subdirectory(player)
endif()

if(SCORE_REMOTE)
  add_subdirectory(remote)
endif()

if(SCORE_COVERAGE)
    add_subdirectory(app-test)
endif()
