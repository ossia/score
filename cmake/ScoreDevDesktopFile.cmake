# The app registers itself with the xdg-desktop-portal under its desktop file
# name; the portal refuses an ID without a matching installed .desktop entry.
# Developer builds install nothing, so give them a hidden user-level entry
# unless a real one (distro package, make install) is already there.
if(NOT UNIX OR APPLE OR EMSCRIPTEN OR ANDROID OR CMAKE_CROSSCOMPILING OR SCORE_DEPLOYMENT_BUILD)
  return()
endif()

option(SCORE_DEV_DESKTOP_FILE "Write ~/.local/share/applications/ossia-score.desktop for developer builds if none is installed" ON)
if(NOT SCORE_DEV_DESKTOP_FILE)
  return()
endif()

if(DEFINED ENV{XDG_DATA_HOME} AND NOT "$ENV{XDG_DATA_HOME}" STREQUAL "")
  set(_score_data_home "$ENV{XDG_DATA_HOME}")
elseif(DEFINED ENV{HOME})
  set(_score_data_home "$ENV{HOME}/.local/share")
else()
  return()
endif()

if(DEFINED ENV{XDG_DATA_DIRS} AND NOT "$ENV{XDG_DATA_DIRS}" STREQUAL "")
  string(REPLACE ":" ";" _score_data_dirs "$ENV{XDG_DATA_DIRS}")
else()
  set(_score_data_dirs /usr/local/share /usr/share)
endif()

set(_score_dev_desktop "${_score_data_home}/applications/ossia-score.desktop")

# An entry written by a developer build is refreshed so that it always points
# at a binary that exists: GLib rejects entries whose Exec cannot be found.
foreach(_dir "${_score_data_home}" ${_score_data_dirs})
  set(_candidate "${_dir}/applications/ossia-score.desktop")
  if(EXISTS "${_candidate}")
    file(STRINGS "${_candidate}" _is_dev REGEX "^X-Score-Developer-Build=true")
    if(NOT _is_dev)
      return()
    endif()
  endif()
endforeach()

# One entry for all configurations: a multi-configuration generator's points
# at the binary of the first one.
get_property(_score_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_score_multi_config AND CMAKE_CONFIGURATION_TYPES)
  list(GET CMAKE_CONFIGURATION_TYPES 0 _score_desktop_config)
  set(_score_desktop_condition "$<CONFIG:${_score_desktop_config}>")
else()
  set(_score_desktop_condition 1)
endif()

file(GENERATE
  OUTPUT "${_score_dev_desktop}"
  CONTENT "[Desktop Entry]
Type=Application
Name=ossia score (developer build)
Exec=$<TARGET_FILE:${APPNAME}> %F
Icon=${CMAKE_SOURCE_DIR}/src/lib/resources/ossia-score.png
Terminal=false
NoDisplay=true
X-Score-Developer-Build=true
"
  CONDITION "${_score_desktop_condition}")
