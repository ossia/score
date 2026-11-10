# Every VST 2.4 plug-in installed on the machine, run through score's host and
# a minimal reference one; see vst_sweep.py. Machine-dependent and long: the
# ctest entry is skipped unless SCORE_VST_SWEEP=1.

if(NOT TARGET score_plugin_vst OR NOT TARGET score_plugin_audio)
  return()
endif()

# score's loader is not exported from a shared plug-in: built in, as in the
# scanner. A static one (macOS, Windows) already brings it.
set(_vst_sweep_loader)
get_target_property(_vst_sweep_plugin_type score_plugin_vst TYPE)
if(NOT _vst_sweep_plugin_type STREQUAL "STATIC_LIBRARY")
  set(_vst_sweep_loader "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst/Vst/Loader.cpp")
endif()

score_add_test(test_vst_sweep_worker
  SOURCES VSTSweepWorker.cpp ${_vst_sweep_loader}
  APP
  NO_CTEST
  PLUGINS score_plugin_vst score_plugin_audio score_lib_process)
target_include_directories(test_vst_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_vst_system_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/vst_sweep.py"
      --worker "$<TARGET_FILE:test_vst_sweep_worker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/vst-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_vst_system_sweep PROPERTIES
    LABELS "vst;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 7200)
endif()
