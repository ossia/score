cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include("${SCORE_SOURCE_DIR}/cmake/ScoreExternalAddon.cmake")
endif()

if(NOT TARGET score_plugin_avnd)
  return()
endif()

project(score_addon_multicursormanager LANGUAGES CXX)

score_common_setup()

avnd_score_plugin_init(
  BASE_TARGET score_addon_multicursormanager
)

avnd_score_plugin_add(
  BASE_TARGET score_addon_multicursormanager
  SOURCES
    MultiCursor/MultiCursor.hpp
    MultiCursor/MultiCursorModel.hpp
    MultiCursor/MultiCursorModel.cpp
  TARGET multicursormanager
  MAIN_CLASS MultiCursorManager
  NAMESPACE Example
)

avnd_score_plugin_finalize(
  BASE_TARGET score_addon_multicursormanager
  PLUGIN_VERSION 1
  PLUGIN_UUID "20f771a1-0e9a-4db2-bb7c-011467d84ded"
)
