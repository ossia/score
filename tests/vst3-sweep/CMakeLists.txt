# Every VST3 plug-in installed on the machine, run through score's host and a
# minimal reference one; see vst3_sweep.py. Machine-dependent and long: the
# ctest entry is skipped unless SCORE_VST3_SWEEP=1.

if(NOT TARGET score_plugin_vst3)
  return()
endif()

score_add_test(test_vst3_sweep_worker
  SOURCES VST3SweepWorker.cpp
  APP
  NO_CTEST
  PLUGINS score_plugin_vst3 score_plugin_dataflow score_plugin_engine
    score_plugin_media score_plugin_audio score_lib_process
  LIBS sdk_common sdk_hosting)
target_include_directories(test_vst3_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst3")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_vst3_system_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/vst3_sweep.py"
      --worker "$<TARGET_FILE:test_vst3_sweep_worker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/vst3-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_vst3_system_sweep PROPERTIES
    LABELS "vst3;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 7200)
endif()
