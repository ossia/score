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

score_add_test(test_integration_js_process
  SOURCES JsProcessTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js)

score_add_test(test_integration_scenario_rename
  SOURCES ScenarioRenameTest.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process)

score_add_test(test_integration_scenario_create
  SOURCES ScenarioCreateTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

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

# A throwing dropCustom handler is contained by the
# noexcept getCustomDrops API.
score_add_test(test_regression_throwing_drop_handler
  SOURCES RegressionThrowingDropHandlerTest.cpp
  APP
  PLUGINS score_lib_process)
