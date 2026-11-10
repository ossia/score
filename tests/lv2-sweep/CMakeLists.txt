# Every LV2 plug-in installed on the machine, run through score's host and a
# minimal reference one; see lv2_sweep.py. Machine-dependent and long: the
# ctest entry is skipped unless SCORE_LV2_SWEEP=1.

if(NOT TARGET score_plugin_lv2)
  return()
endif()

score_add_test(test_lv2_sweep_worker
  SOURCES LV2SweepWorker.cpp
  APP
  NO_CTEST
  PLUGINS score_plugin_lv2 score_lib_process score_plugin_scenario score_plugin_engine
    score_plugin_audio score_plugin_dataflow score_plugin_deviceexplorer
  LIBS Lilv)
target_include_directories(test_lv2_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-lv2"
  "${LV2_PATH}"
  $<TARGET_PROPERTY:Suil,INTERFACE_INCLUDE_DIRECTORIES>
  "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_lv2_system_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lv2_sweep.py"
      --worker "$<TARGET_FILE:test_lv2_sweep_worker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/lv2-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_lv2_system_sweep PROPERTIES
    LABELS "lv2;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 7200)
endif()
