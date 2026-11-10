# Every JSFX effect installed on the machine, run through score's host and
# through the ysfx library used directly; see jsfx_sweep.py.
# Machine-dependent and long: the ctest entry is skipped unless
# SCORE_JSFX_SWEEP=1.

if(NOT TARGET score_plugin_ysfx)
  return()
endif()

score_add_test(test_jsfx_sweep_worker
  SOURCES JSFXSweepWorker.cpp
  APP
  NO_CTEST
  PLUGINS score_plugin_ysfx score_plugin_engine score_plugin_scenario score_lib_process
    score_plugin_audio score_plugin_dataflow score_plugin_deviceexplorer)
target_include_directories(test_jsfx_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-ysfx")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_jsfx_system_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/jsfx_sweep.py"
      --worker "$<TARGET_FILE:test_jsfx_sweep_worker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/jsfx-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_jsfx_system_sweep PROPERTIES
    LABELS "ysfx;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 7200)
endif()
