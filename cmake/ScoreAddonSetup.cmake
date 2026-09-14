# Standalone for the same reason as ScoreAddonArchitecture: ScoreExternalAddon.sdk.cmake
# cannot include ScoreFunctions.cmake, and both add-on paths need what is here.
include("${CMAKE_CURRENT_LIST_DIR}/ScoreAddonArchitecture.cmake")


### The single place localaddon.json is written, shared by setup_score_addon below
### and by avnd_score_plugin_finalize in ScoreAvndHelper.cmake.
function(score_write_addon_manifest)
  cmake_parse_arguments(MANIFEST "" "TARGET;NAME;UUID;VERSION" "" ${ARGN})

  score_addon_architectures(_addon_architectures)
  if(NOT _addon_architectures)
    message(FATAL_ERROR
      "score_write_addon_manifest(${MANIFEST_TARGET}): no add-on architecture key for this platform")
  endif()

  set(_addon_architecture_entries "")
  foreach(_addon_architecture IN LISTS _addon_architectures)
    string(APPEND _addon_architecture_entries
      "  \"${_addon_architecture}\": \"$<TARGET_FILE_NAME:${MANIFEST_TARGET}>\",\n")
  endforeach()

  file(GENERATE OUTPUT plugins/localaddon.json
    CONTENT
      "{
${_addon_architecture_entries}  \"name\": \"${MANIFEST_NAME}\",
  \"raw_name\": \"${MANIFEST_TARGET}\",
  \"version\": \"${MANIFEST_VERSION}\",
  \"kind\": \"addon\",
  \"short\": \"${MANIFEST_NAME}\",
  \"long\": \"${MANIFEST_NAME}\",
  \"key\": \"${MANIFEST_UUID}\"
}"
  )
endfunction()

### Everything setup_score_plugin does, plus the localaddon.json that makes the
### result loadable as a run-time add-on.
###
### setup_score_addon(
###   TARGET   score_addon_foo         # the library
###   [NAME    "Foo"]                  # shown in the add-on manager; default: the target
###   [UUID    "xxxxxxxx-...."]        # the add-on's identity; normally the plug-in's own
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
  # discover at run time and so nothing to describe. That covers both an explicit
  # SCORE_STATIC_PLUGINS build and an add_library() with no type, which is STATIC
  # unless BUILD_SHARED_LIBS says otherwise -- how the templates build in-tree.
  # Emitting a manifest pointing at a .a would describe something score cannot
  # dlopen, so say so and stop rather than writing one.
  get_target_property(_addon_type "${SETUP_ADDON_TARGET}" TYPE)
  if(SCORE_STATIC_PLUGINS OR NOT _addon_type MATCHES "^(MODULE|SHARED)_LIBRARY$")
    message(STATUS
      "score: ${SETUP_ADDON_TARGET} is a ${_addon_type}, so no localaddon.json is generated")
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

  # makeAddon() drops an add-on whose key is not a 36-character uuid, as silently
  # as one with the wrong architecture, so refuse to emit a manifest known to be
  # dead. The value is what PluginDependencyGraph indexes the add-on by, so it
  # should be the uuid the plug-in itself reports.
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

  score_write_addon_manifest(
    TARGET "${SETUP_ADDON_TARGET}"
    NAME "${SETUP_ADDON_NAME}"
    UUID "${SETUP_ADDON_UUID}"
    VERSION "${SETUP_ADDON_VERSION}")

  # Or the add-on installs a library with no manifest beside it.
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugins/localaddon.json
          DESTINATION .)
endfunction()
