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

# Media tests that are allowed to ASSUME a capable host: ffmpeg always, plus
# whatever the requested provisioning needs. Unlike score_add_hardware_test
# these do NOT declare SKIP_RETURN_CODE by default, so an absent dependency
# fails the run instead of quietly vanishing from it.
#
#   VIRTUAL_VIDEO  publish a PipeWire Video/Source from videotestsrc
#   PIPEWIRE       the harness talks to a PipeWire daemon itself, without
#                  needing the wrapper to publish anything into it
#                  (needs gstreamer + a live PipeWire daemon)
#   GSTREAMER      the harness runs its own gst pipelines (needs gstreamer,
#                  but no PipeWire graph)
#   MEDIA          per-pixel-format H.264 / raw clips
#   MATRIX         the container x codec matrix and its known-pixel master
# ctest runs a .sh through an explicit interpreter where the system shell is not
# a POSIX one. On Windows that is msys2's bash, which also carries the ffmpeg and
# GStreamer the harnesses need; elsewhere the shebang suffices and this is empty.
if(WIN32 AND NOT DEFINED SCORE_MEDIA_TEST_SHELL)
  find_program(SCORE_MEDIA_TEST_SHELL NAMES bash
    DOC "POSIX shell used to run the media test harnesses")
endif()

function(score_add_media_test)
  cmake_parse_arguments(ARG
    "VIRTUAL_VIDEO;GSTREAMER;MEDIA;MATRIX;OPTIONAL;PIPEWIRE"
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
  if(ARG_PIPEWIRE)
    list(APPEND _flags --pipewire)
  endif()
  if(ARG_GSTREAMER)
    list(APPEND _flags --gstreamer)
  endif()
  if(ARG_MEDIA)
    list(APPEND _flags --media)
  endif()
  if(ARG_MATRIX)
    list(APPEND _flags --matrix)
  endif()

  # SCORE_MEDIA_TEST_WRAPPER is with-virtual-media.sh. ctest cannot exec a .sh
  # directly on Windows. msys2 supplies bash, ffmpeg and the GStreamer stack,
  # so the harnesses run once they are invoked THROUGH it.
  if(SCORE_MEDIA_TEST_SHELL)
    add_test(NAME ${ARG_NAME}
      COMMAND "${SCORE_MEDIA_TEST_SHELL}" "${SCORE_MEDIA_TEST_WRAPPER}"
              ${_flags} -- "${_exe}" ${ARG_ARGS})
  elseif(SCORE_HAS_SHELL_HARNESS)
    add_test(NAME ${ARG_NAME}
      COMMAND "${SCORE_MEDIA_TEST_WRAPPER}" ${_flags} -- "${_exe}" ${ARG_ARGS})
  else()
    # No shell at all: registering it would report BAD_COMMAND, which reads as a
    # failure of the test rather than of the machine.
    return()
  endif()

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

  # A v4l2loopback + PipeWire graph exists only on Linux. The wrapper already
  # turns a missing prerequisite into exit 77 under SCORE_MEDIA_TESTS_OPTIONAL,
  # so ask for that rather than letting the platform look like a broken test.
  if((ARG_VIRTUAL_VIDEO OR ARG_PIPEWIRE) AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TEST ${ARG_NAME} APPEND PROPERTY
      ENVIRONMENT "SCORE_MEDIA_TESTS_OPTIONAL=1")
    set_property(TEST ${ARG_NAME} PROPERTY SKIP_RETURN_CODE 77)
  endif()

  if(ARG_ENVIRONMENT)
    set_property(TEST ${ARG_NAME} APPEND PROPERTY ENVIRONMENT ${ARG_ENVIRONMENT})
  endif()
endfunction()
