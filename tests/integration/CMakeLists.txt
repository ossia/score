# Integration tests: full headless application + document lifecycle.

# SCORE_HAS_SHELL_HARNESS is set once in cmake/ScoreTests.cmake so that
# tests/hardware sees the same value.
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

# The CPU executor of the Javascript process (JS::js_node), driven port by port
# and tick by tick: audio in and out asserted by sample value, value messages
# with their timestamps, MIDI, buffer-size and channel-count changes, a
# throwing tick, an unparseable script, live script replacement and
# determinism. JsPresetsTest above never touches the node itself.
score_add_test(test_integration_js_cpu_node
  SOURCES JsCpuNodeTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)

# AudioOutlet::setChannel handed something that is not an array converts a NaN
# to int and hands the result to QVector::resize(): the process aborts. Its own
# executable because that abort truncates whatever binary it is in; the test
# turns the abort into exit(1) with its own SIGABRT handler so WILL_FAIL can
# invert it (ctest reports a signal death as "Subprocess aborted" and does not
# invert that, and Catch2's [!shouldfail] never gets to report at all).
# Registered with the CORRECT expectation: when the argument is validated this
# test passes and the ctest entry goes red, which is the signal to drop
# WILL_FAIL and fold the case back into test_integration_js_cpu_node.
score_add_test(test_integration_js_setchannel
  SOURCES JsSetChannelTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)

# The custom-UI half of the Javascript process: a UI script that loads, one
# that does not parse, one whose root is not a ScriptUI, one that imports a
# module that does not exist, no UI at all, and repeated show/hide.
score_add_test(test_integration_js_process_ui
  SOURCES JsProcessUiTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)

# JS::ProcessModel::rootPath() caches the Library settings model in a
# function-local static reference, so the second score application built in a
# process reads the first one's freed settings when a Javascript process is
# created. Its own executable and WILL_FAIL because ASan aborts: registered
# with the CORRECT expectation, so fixing rootPath() turns this entry red.
score_add_test(test_integration_js_rootpath_static
  SOURCES JsRootPathStaticTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)
set_tests_properties(test_integration_js_rootpath_static PROPERTIES WILL_FAIL TRUE)

# Opening a Javascript process UI never emits externalUIVisible(true), so the
# controls that opened it stay in the closed state. [!shouldfail] with the
# correct expectation.
score_add_test(test_integration_js_external_ui_visible
  SOURCES JsExternalUiVisibleTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)

# Which executor a Javascript process gets is decided by the ports its script
# declares (isGpu()): the texture ports must reach the model as Gfx ports and
# editing the script must re-route the process in place. The GPU node's own
# rendering is not covered here.
score_add_test(test_integration_js_gpu_process
  SOURCES JsGpuProcessTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process score_plugin_js score_plugin_library)
if(TARGET score_plugin_gfx)
  target_link_libraries(test_integration_js_gpu_process PRIVATE score_plugin_gfx)
endif()

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

# How the value editors render inside a tree row: fitting the row, covering
# the cell. Writes a PNG per editor to $SCORE_TEST_SHOT_DIR.
score_add_test(test_integration_explorer_editor_look
  SOURCES DeviceExplorerEditorLookTest.cpp
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

  # Every reachable slot of JS::EditJsContext called with null, a wrong-typed
  # argument or a removed object handle, from a real --script run. Subprocess
  # for the same reason: the behaviour under test is whether the application
  # stays alive. Seven launches, so the timeout is generous.
  score_add_test(test_integration_js_scripting_api
    SOURCES JsScriptingApiTest.cpp)
  target_compile_definitions(test_integration_js_scripting_api PRIVATE
    "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
  set_tests_properties(test_integration_js_scripting_api PROPERTIES
    TIMEOUT 900 RUN_SERIAL TRUE)
endif()

# Score.remove() on a cable, which used to be a silent no-op. A subprocess test
# for the same reason: what is under test is the effect on a live document.
if(TARGET score AND NOT EMSCRIPTEN)
  score_add_test(test_integration_js_remove_cable
    SOURCES JsRemoveCableTest.cpp)
  target_compile_definitions(test_integration_js_remove_cable PRIVATE
    "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
  set_tests_properties(test_integration_js_remove_cable PROPERTIES
    TIMEOUT 900 RUN_SERIAL TRUE)
endif()

# Time in the scripting API: the flicks-as-a-number representation scripts
# already depend on, and the positions that were added beside it.
if(TARGET score AND NOT EMSCRIPTEN)
  score_add_test(test_integration_js_time_api
    SOURCES JsTimeApiTest.cpp)
  target_compile_definitions(test_integration_js_time_api PRIVATE
    "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
  set_tests_properties(test_integration_js_time_api PROPERTIES
    TIMEOUT 900 RUN_SERIAL TRUE)
endif()

# PipeWire video input through the whole application, driven by the JS
# scripting API. NO_CTEST: the ctest entries live in tests/hardware, where
# score_add_media_test wraps them in the runner that requires the media stack.
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

  # The gfx protocol factories and their device-settings parsers, reached the
  # way a script reaches them: Score.availableProtocols() and
  # Score.createDevice(name, uuid, obj). No device is expected to connect and
  # nothing is rendered, so unlike the graph tests above this one needs no
  # display and runs under ctest.
  score_add_test(test_integration_gfx_protocol_settings
    SOURCES GfxProtocolSettingsTest.cpp)
  set_tests_properties(test_integration_gfx_protocol_settings
    PROPERTIES TIMEOUT 600)

  foreach(_pw_tgt test_integration_pipewire_video_input
                  test_integration_pipewire_video_motion
                  test_integration_js_graph_e2e
                  test_integration_gfx_protocol_settings)
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
  # One ctest entry per (class, case) rather than per class. The script already
  # takes --cases, and a case is ~20 s: as a single entry per class the group ran
  # 188-423 s each, and one bad case reported as one opaque red covering sixteen.
  # Split, a failure names the case it is.
  foreach(_gr_backend nvidia nvidia-vulkan llvmpipe vulkan-lavapipe)
    string(REPLACE "-" "_" _gr_name "${_gr_backend}")
    set(_gr_cases_file "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/cases-${_gr_backend}.txt")
    if(NOT EXISTS "${_gr_cases_file}")
      set(_gr_cases_file "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/cases-llvmpipe.txt")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_gr_cases_file}")
    file(STRINGS "${_gr_cases_file}" _gr_cases REGEX "^[A-Za-z0-9_-]+$")
    foreach(_gr_case ${_gr_cases})
      add_test(NAME test_golden_render_${_gr_name}.${_gr_case}
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/golden-render.sh"
                --backend ${_gr_backend} --cases ${_gr_case}
        WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
      set_tests_properties(test_golden_render_${_gr_name}.${_gr_case} PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 120
        RUN_SERIAL TRUE
        LABELS "gui;golden-render"
        ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
    endforeach()
  endforeach()
endif()


# The scene testers need the producers their `Wire:` clauses name — Primitive
# cube, Camera, DirectionalLight — which are halp processes in
# score_plugin_threedim, wrapped into score::gfx nodes by oscr::GfxNode<T>. The
# plugin is built with hidden visibility, so (exactly as tests/threedim and
# tests/unit already do) the specific engine .cpp files are compiled straight
# into the test target and the plugin's own include set is pulled in via its
# INCLUDE_DIRECTORIES property. score::gfx::ScenePreprocessorNode and
# oscr::CustomGfxNodeBase are exported by score_plugin_gfx / score_plugin_avnd,
# so those come from the link.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_sweep_scene_hidden
      "${_3d}/Camera.cpp"
      "${_3d}/Light.cpp"
      "${_3d}/Primitive.cpp")
  score_add_test(test_integration_shader_sweep_scene
    SOURCES
      ShaderSweepScene.cpp
      ${_sweep_scene_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_lib_process)
  target_include_directories(test_integration_shader_sweep_scene
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
  target_include_directories(test_integration_shader_sweep_scene SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Encoder matrix: renders through each available graphics API
# (SCORE_TEST_API=opengl|vulkan) and byte-checks the outputs. Registered GUI
# (-> LABELS gui) because GL/Vulkan context creation needs a windowing system;
# exclude with `ctest -LE gui` on displayless machines. Static-plugin builds
# can't link score_plugin_gfx directly (same constraint as EncoderTester).
if(TARGET score_plugin_gfx AND NOT SCORE_STATIC_PLUGINS)
  score_add_test(test_integration_encoder_matrix
    SOURCES EncoderMatrixTest.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_media)
  # GL drivers (llvmpipe in particular) leak internally; don't let LSAN turn
  # that into a test failure.
  set_tests_properties(test_integration_encoder_matrix PROPERTIES
    ENVIRONMENT "SCORE_AUDIO_BACKEND=dummy;ASAN_OPTIONS=detect_leaks=0:halt_on_error=0")
endif()

# On-demand frame stepping: renderFrames / grabFrame / setStepRate and the
# synthetic step clock. Drives the application binary because the mechanism only
# exists inside a real gfx document, and uses separate processes because anything
# cached in one would hide the nondeterminism it looks for. Needs a real display:
# the offscreen QPA has no GL and reads back a flat colour.
if(TARGET score AND TARGET score_plugin_gfx AND NOT EMSCRIPTEN)
  score_add_test(test_integration_frame_determinism
    SOURCES FrameDeterminismTest.cpp
    LIBS ${QT_PREFIX}::Gui)
  target_compile_definitions(test_integration_frame_determinism PRIVATE
    "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\""
    "GFX_TEST_CORPUS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../gfx/corpus\"")
  set_tests_properties(test_integration_frame_determinism PROPERTIES
    TIMEOUT 1800 RUN_SERIAL TRUE LABELS "gui")
endif()

# Manual tool (not a ctest): enumerate and live-preview every registered
# object.  Run:  <build>/tests/integration/ObjectGallery --list
#          or:  cd <build> && ./tests/integration/ObjectGallery --filter camera
add_executable(ObjectGallery "${CMAKE_CURRENT_SOURCE_DIR}/ObjectGallery.cpp")
target_link_libraries(ObjectGallery PRIVATE
  score_lib_base
  score_plugin_scenario score_plugin_engine score_plugin_gfx
  score_plugin_deviceexplorer score_lib_process score_lib_device
  ${QT_PREFIX}::Core ${QT_PREFIX}::Gui ${QT_PREFIX}::Widgets
  ${QT_PREFIX}::Network ${QT_PREFIX}::Xml)
setup_score_common_exe_features(ObjectGallery)
set_target_properties(ObjectGallery PROPERTIES FOLDER "Tests")

# Splat ports stay stable across save/reload
# (score_plugin_threedim is discovered dynamically at runtime).
score_add_test(test_regression_splat_reload
  SOURCES RegressionSplatReloadTest.cpp
  GUI
  PLUGINS score_plugin_scenario score_lib_process)
# ...and the two render-regression sweeps built on it. Both need a real X
# display (llvmpipe over GLX) and skip with 77 without one; csf-sweep also
# needs the out-of-repo shader corpus, keyed off SCORE_SHADER_LIBRARY_DIR
# like the other shader sweeps. Labelled "gui" so display-less CI can gate
# them out, RUN_SERIAL because each spawns a full GL app per case.
if(SCORE_HAS_SHELL_HARNESS)
foreach(sweep object_render_sweep:render-sweep.sh object_csf_sweep:csf-sweep.sh)
  string(REPLACE ":" ";" _s "${sweep}")
  list(GET _s 0 _sweep_name)
  list(GET _s 1 _sweep_script)
  add_test(NAME test_integration_${_sweep_name}
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/${_sweep_script}"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_integration_${_sweep_name} PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 7200
    RUN_SERIAL TRUE
    LABELS "gui;sweep"
    ENVIRONMENT "OBJECT_GALLERY=$<TARGET_FILE:ObjectGallery>;SCORE_AUDIO_BACKEND=dummy;ASAN_OPTIONS=detect_leaks=0:halt_on_error=0")
endforeach()
endif()





# Live graph mutation DURING execution, one ctest entry per scenario. Each
# builds a small ISF scene, plays it headless, and injects mutations over OSC
# while frames flow -- process add/remove storms, cable storms, undo/redo
# storms, transport storms, and window resize/full-screen storms.
#
# This suite existed in the tree but could not run: live-edit-sweep.sh hardcoded
# SRCROOT to a worktree that no longer exists, every scenario eval'd common.js
# through an absolute path into that same dead tree, and common.js pointed at a
# shader corpus under ~/Documents. Nothing registered it with ctest either, so
# none of that was visible. Paths are derived now and the corpus is the in-repo
# one.
#
# The sweep runs on a real X server (Xvfb, else Xephyr) with QT_QPA_PLATFORM=xcb,
# and grabs through the window's own swap chain rather than the desktop, so the
# earlier choice between "vacuous green under the offscreen QPA" and "green by
# screenshotting whatever is in front of the window" no longer applies.
#
# window-storm additionally drives the Window device's parameter tree while the
# scene plays; it is the only entry here that covers GfxContext's clock and
# updateGraph() between the window and the graph.
#
# camera-storm and ndi-storm run that identical viewport storm with a real
# streaming device as the source instead of an ISF, so a failure in one of them
# but not in window-storm belongs to the device rather than to the storm. Both
# skip with a printed reason when the device is absent (no /dev/video*, no
# libndi); a bench machine sets SCORE_REQUIRE_DEVICES=1 to turn that into a
# failure. ndi-storm needs no external sender: score sends and receives its own
# stream over localhost, covering both NDI nodes at once.
#
# Those two are EXPECTED RED on one shared finding: no device pixels reach the
# window through this wiring. Both render the byte-identical disconnected-input
# fallback -- 24% of the frame, the rest black -- from two unrelated sources, so
# the cause is upstream of either device. The storm part passes underneath; it is
# the coverage oracle that fails. Do NOT lower min_nonblack_coverage in
# live-edit-sweep.sh's CFG to make them green: a non-blank check accepts that
# fallback, which is exactly why this looked like a pass to begin with.
if(SCORE_HAS_SHELL_HARNESS)
  foreach(_le_scen baseline add-remove-storm cable-storm undo-redo-during-play
                   transport-storm mixed-chaos window-storm camera-storm ndi-storm)
    add_test(NAME test_live_edit.${_le_scen}
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/live-edit-sweep.sh" ${_le_scen}
      WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
    set_tests_properties(test_live_edit.${_le_scen} PROPERTIES
      SKIP_RETURN_CODE 77
      # Above live-edit-sweep.sh's own `timeout --foreground 150`, so the harness
      # reaches its verdict instead of ctest killing it first and reporting a bare
      # Timeout.
      TIMEOUT 240
      RUN_SERIAL TRUE
      LABELS "live-edit"
      ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
  endforeach()
endif()


# Resource/leak/lifetime soak.
# Creates+destroys 4 gfx processes/cycle during playback via the JS harness and
# asserts exit 0, ASAN-clean, and stable RSS/fd/process counts.
#
# Re-enabled: the render-clock teardown UAF that disabled this is fixed --
# destroyOutput now unregisters the output from every TimerClock before its
# BackgroundNode is freed (GfxContext.cpp). Measured 2026-08-28 at 150 cycles:
# 150/150 executed, zero AddressSanitizer reports, zero teardown errors.
#
# EXPECTED-RED, on a DIFFERENT and real defect: criterion 7, post-warmup RSS
# slope, measures 75.8 KB/cycle against a 40 KB budget. It is monotonic and
# near-linear across the whole run (1339 MB at cycle 20 to 1350 MB at cycle 140,
# ~1530 KB per 20 cycles), so it is a steady leak of roughly 19 KB per gfx
# process created and destroyed, not allocator noise. The summary line prints a
# first-to-last range that can read as a decrease; the per-cycle samples in
# samples.csv are what to trust. Do NOT raise SLOPE_KB_PER_CYCLE to make this
# green -- that is the one thing that would hide the defect it now pins.
# Deterministic-headless -> CI-able. Self-serializes on /tmp/score-harness.lock.
if(SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_gfx_soak
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/gfx-soak/soak-leak.sh" 150
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_gfx_soak PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 1200
    RUN_SERIAL TRUE
    ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
endif()



# Timeline-driven scenario.
# Seeks an automation ramp with OSC /transport, pausing + grabbing at fixed
# positions, and asserts the rendered frame mean == position/duration.
# Deterministic-headless -> CI-able. Self-serializes on /tmp/score-harness.lock.
if(SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_timeline_scenario_ramp
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/timeline-scenarios/timeline-scenario.sh"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_timeline_scenario_ramp PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 600
    RUN_SERIAL TRUE
    ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
endif()



# TEXT node render validation (depends on the scene render path). One
# headless llvmpipe app run: grabs the
# Gfx::Text process with DEFAULT controls (off-screen-default visibility
# regression), then live-edits text/font/size/position/scale/color over OSC
# /script and asserts pixel VALUES per case (analyze.py: coverage, bbox
# ordering across sizes, centroid movement, channel dominance, blank empty
# string, unicode/CJK/tofu/2000-char edge cases) + golden refs for the
# stable subset (text-render/refs/llvmpipe, compare.py strict; regenerate
# with text-render.sh --update-refs — double-render self-consistency).
# Self-serializes on /tmp/score-harness.lock; SKIPs (77) without deps.
if(SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_text_render
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/text-render/text-render.sh"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_text_render PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 900
    RUN_SERIAL TRUE
    ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")
endif()


# Offscreen-teardown regression: a full-app offscreen
# render followed by /stop + /exit must exit 0 (was SIGSEGV/UAF at teardown).
# The script serializes itself on /tmp/score-harness.lock (global OSC port).
if(SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_regression_offscreen_teardown
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/offscreen-teardown-regression.sh"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_regression_offscreen_teardown PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 600
    RUN_SERIAL TRUE
    ENVIRONMENT "SCORE_BIN=${SCORE_ROOT_BINARY_DIR}/ossia-score")
endif()



# Video decode-correctness matrix: every SOFTWARE-
# decodable pixel format in Gfx/Graph/decoders is encoded as a known pattern
# (ffprobe-verified pix_fmt), played through VideoDecoderTester with
# --expect/--psnr, and the rendered RGBA is asserted against ffmpeg's own
# decode of the same clip (per-format PSNR bound), plus truncated/garbage
# fuzz clips asserting graceful rejection. Needs an X display (llvmpipe over
# GLX — the Qt 6.4 offscreen QPA has no headless GL/Vulkan): labeled gui,
# skips (77) without ffmpeg/tester/display. Each tester run self-serializes
# on /tmp/score-harness.lock.
if(SCORE_VIDEO_TESTER)
  add_test(NAME test_video_decode_correctness
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/video-decode-correctness.sh"
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_video_decode_correctness PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 3600
    RUN_SERIAL TRUE
    LABELS "gui"
    ENVIRONMENT "VIDEO_TESTER=${SCORE_ROOT_BINARY_DIR}/VideoDecoderTester;SCORE_AUDIO_BACKEND=dummy;ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0")
endif()
