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
add_test(NAME test_frame_determinism
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/frame-determinism.sh"
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_frame_determinism PROPERTIES
  SKIP_RETURN_CODE 77
  TIMEOUT 600
  RUN_SERIAL TRUE
  LABELS "gui"
  ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

# Golden-image render regression.
# Renders a pinned subset of the tests-scene JS corpus headless on llvmpipe and
# compares each frame to refs/llvmpipe/*.png with compare.py (SSIM/PSNR).
# Deterministic-headless -> CI-able. References are committed under
# refs/llvmpipe/; regenerate with:  golden-render.sh --backend llvmpipe --update-refs
# Self-serializes on /tmp/score-harness.lock; skips (77) if refs/oscsend/corpus absent.
add_test(NAME test_golden_render_llvmpipe
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/golden-render/golden-render.sh" --backend llvmpipe
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_golden_render_llvmpipe PROPERTIES
  SKIP_RETURN_CODE 77
  TIMEOUT 1800
  RUN_SERIAL TRUE
  ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

# Resource/leak/lifetime soak.
# Creates+destroys 4 gfx processes/cycle during playback via the JS harness and
# asserts exit 0, ASAN-clean, and stable RSS/fd/process counts.
# DISABLED: this reproducer currently FAILS on a REAL, unfixed heap-use-after-
# free in the render-clock teardown path — GfxContext.cpp:341 (the TimerClock
# tick lambda dereferences an offscreen BackgroundNode already freed by
# ~offscreen_device, OffscreenDevice.hpp:97) fired from forceCloseDocument's
# event pump on /exit. This is NEW render-clock code (RenderClock.cpp/TimerClock),
# distinct from the 3 previously-fixed teardown UAFs. Flip DISABLED->FALSE once
# that UAF is fixed; it then becomes the leak/teardown regression gate.
# Deterministic-headless -> CI-able. Self-serializes on /tmp/score-harness.lock.
add_test(NAME test_gfx_soak
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/gfx-soak/soak-leak.sh" 150
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_gfx_soak PROPERTIES
  DISABLED TRUE
  SKIP_RETURN_CODE 77
  TIMEOUT 1200
  RUN_SERIAL TRUE
  ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

# Timeline-driven scenario.
# Seeks an automation ramp with OSC /transport, pausing + grabbing at fixed
# positions, and asserts the rendered frame mean == position/duration.
# Deterministic-headless -> CI-able. Self-serializes on /tmp/score-harness.lock.
add_test(NAME test_timeline_scenario_ramp
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/timeline-scenarios/timeline-scenario.sh"
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_timeline_scenario_ramp PROPERTIES
  SKIP_RETURN_CODE 77
  TIMEOUT 600
  RUN_SERIAL TRUE
  ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

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
add_test(NAME test_text_render
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/text-render/text-render.sh"
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_text_render PROPERTIES
  SKIP_RETURN_CODE 77
  TIMEOUT 900
  RUN_SERIAL TRUE
  ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")

# GPU video encoder matrix: instantiates every encoder in
# src/plugins/score-plugin-gfx/Gfx/Graph/encoders on a real QRhi backend
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

# Offscreen-teardown regression: a full-app offscreen
# render followed by /stop + /exit must exit 0 (was SIGSEGV/UAF at teardown).
# The script serializes itself on /tmp/score-harness.lock (global OSC port).
add_test(NAME test_regression_offscreen_teardown
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/offscreen-teardown-regression.sh"
  WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
set_tests_properties(test_regression_offscreen_teardown PROPERTIES
  SKIP_RETURN_CODE 77
  TIMEOUT 600
  RUN_SERIAL TRUE
  ENVIRONMENT "SCORE_BIN=${SCORE_ROOT_BINARY_DIR}/ossia-score")

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
