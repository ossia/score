# Standalone on purpose, like ScoreAddonArchitecture: ScoreExternalAddon.sdk.cmake
# cannot include ScoreFunctions.cmake, so anything both the developer-tree and
# the SDK add-on paths need has to live outside it. setup_score_addon used to sit
# in ScoreExternalAddon.developer.cmake and was therefore unreachable from an SDK
# build -- which is why a non-Avendish add-on built against the SDK produced a
# library and no manifest, and so could never be installed.
include("${CMAKE_CURRENT_LIST_DIR}/ScoreAddonArchitecture.cmake")

### Everything setup_score_plugin does, plus the localaddon.json that makes the
### result loadable as a run-time add-on.
###
### setup_score_addon(
###   TARGET   score_addon_foo         # the library
###   [NAME    "Foo"]                  # shown in the add-on manager; default: the target
###   [UUID    "xxxxxxxx-...."]        # must equal the uuid the plug-in reports
###   [VERSION 1]
###   [METADATA addon.json]            # default: addon.json next to the CMakeLists
### )
###
### UUID may be omitted when METADATA carries a "key": the manifest has exactly
### one source of truth either way.
function(setup_score_addon)
  cmake_parse_arguments(SETUP_ADDON "" "TARGET;NAME;UUID;VERSION;METADATA" "" ${ARGN})

  if(NOT SETUP_ADDON_TARGET)
    message(FATAL_ERROR "setup_score_addon: TARGET is required")
  endif()
  if(NOT TARGET "${SETUP_ADDON_TARGET}")
    message(FATAL_ERROR "setup_score_addon: '${SETUP_ADDON_TARGET}' is not a target")
  endif()

  setup_score_plugin("${SETUP_ADDON_TARGET}")

  # A statically linked plug-in is part of the application; there is nothing to
  # discover at run time and so nothing to describe.
  if(SCORE_STATIC_PLUGINS)
    return()
  endif()

  if(NOT SETUP_ADDON_NAME)
    set(SETUP_ADDON_NAME "${SETUP_ADDON_TARGET}")
  endif()
  if(NOT SETUP_ADDON_VERSION)
    set(SETUP_ADDON_VERSION "1")
  endif()
  if(NOT SETUP_ADDON_METADATA)
    set(SETUP_ADDON_METADATA "${CMAKE_CURRENT_SOURCE_DIR}/addon.json")
  endif()

  # The uuid is what score matches against the one the plug-in reports; a wrong
  # or absent one makes makeAddon() discard the add-on without a word, so refuse
  # to emit a manifest that is known to be unloadable.
  if(NOT SETUP_ADDON_UUID AND EXISTS "${SETUP_ADDON_METADATA}")
    file(READ "${SETUP_ADDON_METADATA}" _addon_metadata)
    string(JSON SETUP_ADDON_UUID ERROR_VARIABLE _json_err GET "${_addon_metadata}" "key")
  endif()
  string(LENGTH "${SETUP_ADDON_UUID}" _uuid_length)
  if(NOT _uuid_length EQUAL 36)
    message(FATAL_ERROR
      "setup_score_addon(${SETUP_ADDON_TARGET}): no 36-character uuid. Pass UUID, or give "
      "'${SETUP_ADDON_METADATA}' a \"key\". Without it score drops the add-on silently at load time.")
  endif()

  score_addon_architectures(_addon_architectures)
  if(NOT _addon_architectures)
    message(FATAL_ERROR
      "setup_score_addon(${SETUP_ADDON_TARGET}): no add-on architecture key for this platform")
  endif()

  set(_addon_architecture_entries "")
  foreach(_addon_architecture IN LISTS _addon_architectures)
    string(APPEND _addon_architecture_entries
      "  \"${_addon_architecture}\": \"$<TARGET_FILE_NAME:${SETUP_ADDON_TARGET}>\",\n")
  endforeach()

  file(GENERATE OUTPUT plugins/localaddon.json
    CONTENT
      "{
${_addon_architecture_entries}  \"name\": \"${SETUP_ADDON_NAME}\",
  \"raw_name\": \"${SETUP_ADDON_TARGET}\",
  \"version\": \"${SETUP_ADDON_VERSION}\",
  \"kind\": \"addon\",
  \"short\": \"${SETUP_ADDON_NAME}\",
  \"long\": \"${SETUP_ADDON_NAME}\",
  \"key\": \"${SETUP_ADDON_UUID}\"
}"
  )
endfunction()
