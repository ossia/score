# ossia score — hardware-gated test registration.
#
# Round-trip harnesses that drive real capture/playout devices (AJA, DeckLink,
# Magewell, PipeWire, …) must be part of the regular ctest suite so the
# dedicated rigs exercise them, while every other machine skips them *cleanly*
# (SKIP, not FAIL, not silently-absent).
#
# Usage:
#   include(ScoreHardwareTests)
#   score_add_hardware_test(
#     NAME       PipewireRoundtrip                  # ctest entry name
#     EXECUTABLE PipewireRoundtrip                  # target name or abs. path
#     VENDOR     pipewire                           # -> LABELS "hardware;<vendor>"
#     PROBE      "${CMAKE_CURRENT_SOURCE_DIR}/probe-pipewire.sh"  # cheap check
#     [ARGS      --seconds 2 ...]                   # harness arguments
#     [TIMEOUT   900]                               # seconds (default 600)
#     [ENVIRONMENT VAR=value ...])                  # extra test environment
#
# Semantics:
#   - The registered test command is tests/hardware/run-hardware-test.sh,
#     which first runs PROBE (a shell command; typically one of the
#     tests/hardware/probe-*.sh scripts). If the probe fails, the wrapper
#     exits 77 and ctest reports the test as ***Skipped (SKIP_RETURN_CODE).
#     If the probe succeeds, the wrapper exec()s the real harness.
#   - LABELS "hardware;<vendor>": run only the rig-relevant subset with e.g.
#     `ctest -L aja`, or exclude all hardware tests with `ctest -LE hardware`.
#   - RUN_SERIAL: harnesses grab exclusive device channels (and the PipeWire
#     graph); never run two hardware tests concurrently.
#   - A hung harness is killed by ctest after TIMEOUT and reported as a
#     failure — on a rig with the device present, a hang IS a failure.

include_guard(GLOBAL)

set(SCORE_HARDWARE_TEST_WRAPPER
    "${SCORE_ROOT_SOURCE_DIR}/tests/hardware/run-hardware-test.sh"
    CACHE INTERNAL "Probe-then-exec wrapper for hardware-gated tests")

set(SCORE_MEDIA_TEST_WRAPPER
    "${SCORE_ROOT_SOURCE_DIR}/tests/hardware/with-virtual-media.sh"
    CACHE INTERNAL "provision-then-exec wrapper for media tests")

function(score_add_hardware_test)
  cmake_parse_arguments(ARG
    ""
    "NAME;EXECUTABLE;VENDOR;TIMEOUT"
    "PROBE;ARGS;ENVIRONMENT"
    ${ARGN})

  if(NOT ARG_NAME OR NOT ARG_EXECUTABLE OR NOT ARG_PROBE)
    message(FATAL_ERROR
      "score_add_hardware_test: NAME, EXECUTABLE and PROBE are required")
  endif()
  if(NOT ARG_VENDOR)
    set(ARG_VENDOR generic)
  endif()
  if(NOT ARG_TIMEOUT)
    set(ARG_TIMEOUT 600)
  endif()

  # EXECUTABLE: prefer a target's real output file; fall back to a path.
  if(TARGET "${ARG_EXECUTABLE}")
    set(_exe "$<TARGET_FILE:${ARG_EXECUTABLE}>")
  else()
    set(_exe "${ARG_EXECUTABLE}")
  endif()

  # PROBE may be given as a list; it is executed as one `sh -c` command line.
  list(JOIN ARG_PROBE " " _probe)

  add_test(NAME ${ARG_NAME}
    COMMAND "${SCORE_HARDWARE_TEST_WRAPPER}" "${_probe}" -- "${_exe}" ${ARG_ARGS})

  set_tests_properties(${ARG_NAME} PROPERTIES
    SKIP_RETURN_CODE 77
    LABELS "hardware;${ARG_VENDOR}"
    TIMEOUT "${ARG_TIMEOUT}"
    RUN_SERIAL TRUE
    # Dynamic-plugin builds discover plugins from "<cwd>/plugins".
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}"
    ENVIRONMENT "SCORE_AUDIO_BACKEND=dummy;SCORE_DISABLE_AUDIOPLUGINS=1")

  if(ARG_ENVIRONMENT)
    set_property(TEST ${ARG_NAME} APPEND PROPERTY
      ENVIRONMENT ${ARG_ENVIRONMENT})
  endif()
endfunction()

# Media tests that are allowed to ASSUME a capable host: gstreamer, ffmpeg and
# a live PipeWire daemon. Unlike score_add_hardware_test these do NOT declare
# SKIP_RETURN_CODE by default, so an absent dependency fails the run instead of
# quietly vanishing from it. VIRTUAL_VIDEO provisions a PipeWire Video/Source
# from videotestsrc; MEDIA generates H.264 and raw clips with ffmpeg.
function(score_add_media_test)
  cmake_parse_arguments(ARG
    "VIRTUAL_VIDEO;MEDIA;OPTIONAL"
    "NAME;EXECUTABLE;TIMEOUT"
    "ARGS;ENVIRONMENT"
    ${ARGN})

  if(NOT ARG_NAME OR NOT ARG_EXECUTABLE)
    message(FATAL_ERROR "score_add_media_test: NAME and EXECUTABLE are required")
  endif()
  if(NOT ARG_TIMEOUT)
    set(ARG_TIMEOUT 600)
  endif()
  if(TARGET "${ARG_EXECUTABLE}")
    set(_exe "$<TARGET_FILE:${ARG_EXECUTABLE}>")
  else()
    set(_exe "${ARG_EXECUTABLE}")
  endif()

  set(_flags "")
  if(ARG_VIRTUAL_VIDEO)
    list(APPEND _flags --video)
  endif()
  if(ARG_MEDIA)
    list(APPEND _flags --media)
  endif()

  add_test(NAME ${ARG_NAME}
    COMMAND "${SCORE_MEDIA_TEST_WRAPPER}" ${_flags} -- "${_exe}" ${ARG_ARGS})

  set_tests_properties(${ARG_NAME} PROPERTIES
    LABELS "media"
    TIMEOUT "${ARG_TIMEOUT}"
    RUN_SERIAL TRUE
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}"
    ENVIRONMENT "SCORE_AUDIO_BACKEND=dummy;SCORE_DISABLE_AUDIOPLUGINS=1")

  # Only an explicitly OPTIONAL test may skip; everything else must run.
  if(ARG_OPTIONAL)
    set_property(TEST ${ARG_NAME} PROPERTY SKIP_RETURN_CODE 77)
  endif()

  if(ARG_ENVIRONMENT)
    set_property(TEST ${ARG_NAME} APPEND PROPERTY ENVIRONMENT ${ARG_ENVIRONMENT})
  endif()
endfunction()
