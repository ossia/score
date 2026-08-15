# Integration tests: full headless application + document lifecycle.
# These run from the build root so the dynamic plugins in <build>/plugins are
# discovered at runtime (handled by score_add_test ... APP).

score_add_test(test_integration_document
  SOURCES DocumentLifecycleTest.cpp
  APP)

score_add_test(test_integration_roundtrip
  SOURCES DocumentRoundtripTest.cpp
  APP)

score_add_test(test_integration_scenario_undo
  SOURCES ScenarioUndoTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

score_add_test(test_integration_scenario_roundtrip
  SOURCES ScenarioContentRoundtripTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# Project consolidation end to end: collect a document's media into its own
# folder and repoint it. GUI because adding a process to an interval goes
# through the document presenter.
if(TARGET score_plugin_media)
  score_add_test(test_integration_consolidation
    SOURCES ProjectConsolidationTest.cpp
    GUI
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)
endif()

score_add_test(test_integration_js_process
  SOURCES JsProcessTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js)

score_add_test(test_integration_scenario_rename
  SOURCES ScenarioRenameTest.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process)

# How a node displays a process with far more controls than fits on screen.
score_add_test(test_integration_nodal_controls
  SOURCES NodalControlsTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# Where nodes land when score picks the position itself (library double-click,
# "create after this process", drops on a cable).
score_add_test(test_integration_nodal_placement
  SOURCES NodalPlacementTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_plugin_automation score_plugin_library
          score_lib_process)

score_add_test(test_integration_scenario_create
  SOURCES ScenarioCreateTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# Device explorer address panel: editing values, accepted values, and the
# absence of empty undo steps.
score_add_test(test_integration_device_address_edit
  SOURCES DeviceAddressEditTest.cpp
  GUI
  PLUGINS score_plugin_deviceexplorer score_lib_device score_plugin_scenario)

# --- Regression tests for the crash/robustness fixes in this tree. Each
# test names the fix it guards; see the test
# sources for the failure mode without the fix.

# Null document-presenter guard on selection change.
score_add_test(test_regression_null_presenter_selection
  SOURCES RegressionNullPresenterSelectionTest.cpp
  APP)

# No libpd patch open when the script path is empty.
score_add_test(test_regression_pd_empty_script
  SOURCES RegressionPdEmptyScriptTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# MinimalApplication static default arguments — two full
# construct/destroy cycles in one process.
score_add_test(test_regression_minimal_app_twice
  SOURCES RegressionMinimalAppTwiceTest.cpp
  APP)

# A declining PanelDelegateFactory must not register
# a null panel (GUIApplicationRegistrar::registerPanel path).
score_add_test(test_regression_panel_decline
  SOURCES RegressionPanelDeclineTest.cpp
  APP)

# Same guard on the registerPlugin/loadPluginData
# panel-instantiation path (needs a view, hence GUI).
score_add_test(test_regression_panel_plugin_load
  SOURCES RegressionPanelPluginLoadTest.cpp
  GUI)

# Null-tolerant device-explorer panel lookup.
score_add_test(test_regression_panel_null_slot_lookup
  SOURCES RegressionPanelNullSlotLookupTest.cpp
  GUI
  PLUGINS score_plugin_deviceexplorer)

# The event queue is drained before the presenter is
# deleted (deferred slots read presenter-owned memory).
score_add_test(test_regression_presenter_drain
  SOURCES RegressionPresenterDrainTest.cpp
  APP)

# A second exit request during or after shutdown is a no-op
# (re-entering closeAllDocuments aborted the process).
score_add_test(test_regression_double_exit
  SOURCES RegressionDoubleExitTest.cpp
  APP)

# A throwing dropCustom handler is contained by the
# noexcept getCustomDrops API.
score_add_test(test_regression_throwing_drop_handler
  SOURCES RegressionThrowingDropHandlerTest.cpp
  APP
  PLUGINS score_lib_process)

# One harness per shader kind in the library: they are authored differently,
# fail differently, and each keeps its own baseline. All need a GPU-capable
# display, and all skip when no library is installed.
foreach(kind ISF VSA CSF)
  string(TOLOWER "${kind}" lc)
  score_add_test(test_integration_shader_sweep_${lc}
    SOURCES ShaderSweep${kind}.cpp
    GUI
    PLUGINS score_plugin_gfx score_lib_process)
  set(_baseline "${CMAKE_CURRENT_SOURCE_DIR}/../testdata/shader_sweep_baseline_${lc}.txt")

  # On wasm there is no filesystem to read the library or the baseline from:
  # bake both into the test's own MEMFS image, the way the application does.
  if(EMSCRIPTEN)
    target_link_options(test_integration_shader_sweep_${lc} PRIVATE
      "SHELL:--preload-file ${_baseline}@/shader_sweep_baseline_${lc}.txt")
    set(_baseline "/shader_sweep_baseline_${lc}.txt")

    if(SCORE_WASM_PRELOAD_LIBRARY)
      target_link_options(test_integration_shader_sweep_${lc} PRIVATE
        "SHELL:--preload-file ${SCORE_WASM_PRELOAD_LIBRARY}@/score-library")
      target_compile_definitions(test_integration_shader_sweep_${lc} PRIVATE
        "SCORE_SHADER_SWEEP_WASM_LIBRARY=\"/score-library\"")
    endif()
  endif()

  target_compile_definitions(test_integration_shader_sweep_${lc} PRIVATE
    "SCORE_SHADER_SWEEP_BASELINE_${kind}=\"${_baseline}\"")
endforeach()

# Drags that never get their mouse release: QGraphicsScene drops the implicit
# grab on a button-less move, so the control keeps its ongoing command open and
# redirects the next control's edits onto itself.
score_add_test(test_integration_lost_mouse_grab
  SOURCES LostMouseGrabTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_fx)

# Fold state lives on the process so that it survives save/reload and follows a
# preset dropped onto an open node.
score_add_test(test_integration_process_fold_mode
  SOURCES ProcessFoldModeTest.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process score_plugin_fx)
