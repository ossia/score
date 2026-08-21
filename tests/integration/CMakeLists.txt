# Integration tests: full headless application + document lifecycle.

# The .sh harnesses need flock/oscsend and POSIX tooling. On Windows ctest
# cannot execute them at all and reports BAD_COMMAND, which counts as a failure
# rather than a skip.
if(WIN32)
  set(SCORE_HAS_SHELL_HARNESS 0)
else()
  set(SCORE_HAS_SHELL_HARNESS 1)
endif()
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

# The File > Project files submenu: present, placed, and unambiguous.
score_add_test(test_integration_project_files_menu
  SOURCES ProjectFilesMenuTest.cpp
  GUI
  PLUGINS score_lib_process)

# Project consolidation end to end: collect a document's media into its own
# folder and repoint it. GUI because adding a process to an interval goes
# through the document presenter.
if(TARGET score_plugin_media)
  score_add_test(test_integration_consolidation
    SOURCES ProjectConsolidationTest.cpp
    GUI
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)

  # Finding, and re-finding, the media a document points at.
  score_add_test(test_integration_missing_files
    SOURCES MissingFilesTest.cpp
    GUI
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)

  # Trimming is the one operation here that can destroy audio: it runs with
  # the filesystem read-only outside /tmp so a regression cannot reach any
  # real file. See tests/tools/sandboxed-test.sh.
  score_add_test(test_integration_media_trim
    SOURCES MediaTrimTest.cpp
    GUI
    SANDBOXED
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)

  # Clearing leftovers out of the project folder. Deletes files, so sandboxed.
  score_add_test(test_integration_unused_files
    SOURCES UnusedFilesTest.cpp
    GUI
    SANDBOXED
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)

  # Packing a whole project into a single zip.
  score_add_test(test_integration_project_archive
    SOURCES ProjectArchiveTest.cpp
    GUI
    PLUGINS score_plugin_scenario score_plugin_media score_lib_process)
endif()

score_add_test(test_integration_js_process
  SOURCES JsProcessTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js)

# Sweeps every Javascript preset in the user library through the real QML
# engine: parses it, ticks it, and checks it never throws or emits a NaN.
# Skips itself when the user library is not installed.
score_add_test(test_integration_js_presets
  SOURCES JsPresetsTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)

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

# --script failure reporting: exit 0 / N / 2 / 3 and the diagnostics that go
# with them. A subprocess test because the behaviour under test IS the exit
# code of the application binary.
if(TARGET score AND NOT EMSCRIPTEN)
  score_add_test(test_integration_script_exit_code
    SOURCES ScriptExitCodeTest.cpp)
  target_compile_definitions(test_integration_script_exit_code PRIVATE
    "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
  set_tests_properties(test_integration_script_exit_code PROPERTIES
    TIMEOUT 900 RUN_SERIAL TRUE)
endif()

# PipeWire video input through the whole application, driven by the JS
# scripting API. NO_CTEST: the ctest entries live in tests/hardware, where
# score_add_media_test wraps them in the runner that requires the media stack.
#
# PipewireVideoInputTest.cpp has been in the tree without this since it was
# written: nothing created the target, so the `if(TARGET
# test_integration_pipewire_video_input)` guard in tests/hardware/CMakeLists.txt
# never fired and Media_PipewireVideoInput was never a test.
#
# The motion test is the one that can tell a live stream from a still: its
# source publishes a frame-numbered pattern the test wrote itself, so both the
# pixels and the fact that they change are asserted against a reference that
# owes nothing to score.
if(TARGET score AND TARGET score_plugin_gfx AND NOT EMSCRIPTEN)
  score_add_test(test_integration_pipewire_video_input
    SOURCES PipewireVideoInputTest.cpp
    NO_CTEST)
  score_add_test(test_integration_pipewire_video_motion
    SOURCES PipewireVideoMotionTest.cpp
    NO_CTEST)

  # Video graphs assembled entirely through the JS scripting API and asserted
  # on their pixels: file -> filter -> window, one source into two windows, and
  # the same graph built and torn down repeatedly in one process.
  score_add_test(test_integration_js_graph_e2e
    SOURCES JsGraphE2ETest.cpp
    NO_CTEST)

  foreach(_pw_tgt test_integration_pipewire_video_input
                  test_integration_pipewire_video_motion
                  test_integration_js_graph_e2e)
    target_link_libraries(${_pw_tgt} PRIVATE ${QT_PREFIX}::Gui)
    target_compile_definitions(${_pw_tgt} PRIVATE
      "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\""
      "GFX_TEST_CORPUS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../gfx/corpus\"")
  endforeach()
endif()

# Closing a MODIFIED document with applicationSettings.gui false must not
# raise the save prompt: nothing can answer it and QMessageBox::exec() aborts.
score_add_test(test_regression_headless_close_modified
  SOURCES RegressionHeadlessCloseModifiedTest.cpp
  APP)
# Without the guard the modal is raised and blocks forever under the offscreen
# QPA: the regression shows up as a hang, so it needs a deadline to be a
# failure rather than a stuck job.
set_tests_properties(test_regression_headless_close_modified PROPERTIES TIMEOUT 120)

# A throwing dropCustom handler is contained by the
# noexcept getCustomDrops API.
score_add_test(test_regression_throwing_drop_handler
  SOURCES RegressionThrowingDropHandlerTest.cpp
  APP
  PLUGINS score_lib_process)

# One harness per shader kind in the library: they are authored differently,
# fail differently, and each keeps its own baseline. All need a GPU-capable
# display, and all skip when no library is installed.
foreach(kind ISF VSA CSF Raster CSFGeometry Wired)
  string(TOLOWER "${kind}" lc)
  string(TOUPPER "${kind}" uc)
  score_add_test(test_integration_shader_sweep_${lc}
    SOURCES ShaderSweep${kind}.cpp
    GUI
    PLUGINS score_plugin_gfx score_lib_process)
  # Both of these drive render_raster() from the gfx fixture. Raster varies the
  # rasterizer over a fixed geometry producer; CSFGeometry varies the producer
  # over a fixed rasterizer — no .cs in the corpus declares an OUTPUTS block, so
  # an Image sink can never see what a compute shader writes.
  # Wired hand-builds the graph each tester's `Wire:` clause prescribes, one
  # fixture per tester, so it asserts directly instead of keeping a baseline.
  if(kind STREQUAL "Raster" OR kind STREQUAL "CSFGeometry" OR kind STREQUAL "Wired")
    target_include_directories(test_integration_shader_sweep_${lc}
      PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
  endif()
  if(EMSCRIPTEN AND SCORE_WASM_PRELOAD_LIBRARY)
    target_link_options(test_integration_shader_sweep_${lc} PRIVATE
      "SHELL:--preload-file ${SCORE_WASM_PRELOAD_LIBRARY}@/score-library")
    target_compile_definitions(test_integration_shader_sweep_${lc} PRIVATE
      "SCORE_SHADER_SWEEP_WASM_LIBRARY=\"/score-library\"")
  endif()

  if(NOT kind STREQUAL "Wired")
    set(_baseline "${CMAKE_CURRENT_SOURCE_DIR}/../testdata/shader_sweep_baseline_${lc}.txt")

    # On wasm there is no filesystem to read the baseline from: bake it into the
    # test's own MEMFS image, the way the application does.
    if(EMSCRIPTEN)
      target_link_options(test_integration_shader_sweep_${lc} PRIVATE
        "SHELL:--preload-file ${_baseline}@/shader_sweep_baseline_${lc}.txt")
      set(_baseline "/shader_sweep_baseline_${lc}.txt")
    endif()

    target_compile_definitions(test_integration_shader_sweep_${lc} PRIVATE
      "SCORE_SHADER_SWEEP_BASELINE_${uc}=\"${_baseline}\"")
  endif()
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

# What a press next to a port hits: the port's hit zone is a circle around the
# drawn circle, not a square spilling onto the control beside it; and cables
# anchor on the center of that circle.
score_add_test(test_integration_port_hit_zone
  SOURCES PortHitZoneTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# Dragging a cable against the edge of the central view reveals more of the
# score, in the timeline (scene grows) and on the nodal canvas (pans).
score_add_test(test_integration_cable_drag_view_scroll
  SOURCES CableDragViewScrollTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# The nodal canvas' zoom and position come back as saved, from the JSON and
# binary formats, whatever happened to the nodes since; older documents open
# centered on their nodes.
score_add_test(test_integration_nodal_viewport
  SOURCES NodalViewportTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)

# Frame-step determinism across two processes: the same case rendered to the same
# frame index in two separate applications must be byte-identical.
# Skips (77) without the binary or the out-of-repo tests-scene corpus.
if(SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_frame_determinism
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/frame-determinism.sh"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_frame_determinism PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 600
    RUN_SERIAL TRUE
    LABELS "gui"
    ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

  # Golden-image render regression, one ctest per backend class. Each renders the
  # pinned tests-scene corpus through the real binary and compares against
  # golden-render/refs/<class>/. Skips (77) without the binary, the out-of-repo
  # tests-scene corpus, oscsend/ImageMagick/numpy, an X server for the classes
  # that need one, or a not-yet-generated reference set.
  #
  # The backend a run actually got is asserted, not assumed: a class that lands on
  # a different GPU or rasteriser fails as WRONG-BACKEND. Without that assertion a
  # run under QT_QPA_PLATFORM=offscreen falls back to the Null RHI backend and
  # still emits a plausible, perfectly reproducible, WRONG picture.
  foreach(_gr_backend nvidia nvidia-vulkan llvmpipe vulkan-lavapipe)
    string(REPLACE "-" "_" _gr_name "${_gr_backend}")
    add_test(NAME test_golden_render_${_gr_name}
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/golden-render.sh"
              --backend ${_gr_backend}
      WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
    set_tests_properties(test_golden_render_${_gr_name} PROPERTIES
      SKIP_RETURN_CODE 77
      TIMEOUT 1800
      RUN_SERIAL TRUE
      LABELS "gui"
      ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
  endforeach()
endif()
