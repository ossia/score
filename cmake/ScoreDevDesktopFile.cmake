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
")
