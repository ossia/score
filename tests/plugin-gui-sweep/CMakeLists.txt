# The editors of every plug-in installed on the machine, opened through score's
# own windows on a private X server and driven with the pointer; see
# plugin_gui_sweep.py. Machine-dependent and long: the ctest entry is skipped
# unless SCORE_PLUGIN_GUI_SWEEP=1.

if(WIN32 OR APPLE OR EMSCRIPTEN)
  return()
endif()
foreach(_plugin score_plugin_clap score_plugin_vst score_plugin_vst3 score_plugin_lv2
                score_plugin_ysfx score_plugin_engine)
  if(NOT TARGET ${_plugin})
    return()
  endif()
endforeach()
find_library(SCORE_GUI_SWEEP_XCB xcb)
if(NOT SCORE_GUI_SWEEP_XCB)
  return()
endif()

score_add_test(test_plugin_gui_sweep_worker
  # score's VST 2 loader is not exported: built in, as in the scanner
  SOURCES PluginGuiSweepWorker.cpp
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst/Vst/Loader.cpp"
  GUI
  NO_CTEST
  PLUGINS score_plugin_clap score_plugin_vst score_plugin_vst3 score_plugin_lv2
    score_plugin_ysfx score_plugin_engine score_plugin_scenario score_plugin_media
    score_plugin_audio score_plugin_dataflow score_plugin_deviceexplorer score_lib_process
  LIBS Lilv sdk_common sdk_hosting ${SCORE_GUI_SWEEP_XCB} ${CMAKE_DL_LIBS})
target_include_directories(test_plugin_gui_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-clap"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst3"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-lv2"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-ysfx"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/clap/include"
  "${LV2_PATH}"
  $<TARGET_PROPERTY:Suil,INTERFACE_INCLUDE_DIRECTORIES>
  "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_plugin_gui_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/plugin_gui_sweep.py"
      --worker "$<TARGET_FILE:test_plugin_gui_sweep_worker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/plugin-gui-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_plugin_gui_sweep PROPERTIES
    LABELS "plugin;gui;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 36000)
endif()
