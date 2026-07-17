# Unit tests: pure logic, no application context. Fast to build and run.

score_add_test(test_unit_address
  SOURCES AddressParseTest.cpp
  PLUGINS score_lib_state)

# --- project consolidation primitives (score_lib_base) ---------------------
# Path math against explicit roots, file-name sanitizing, destination
# placement. No document, no app: this is the layer everything else builds on.
score_add_test(test_unit_project_files
  SOURCES ProjectFilesTest.cpp)

# The document-free score::locateFilePath(QString) overload: <LIBRARY>: out of
# QSettings, with no roots. A different function from the ProjectFiles.hpp one
# above despite the shared name.
score_add_test(test_unit_locate_file_path
  SOURCES LocateFilePathTest.cpp)

# Zip writing (miniz, vendored in 3rdparty/zipdownloader) behind score's API.
score_add_test(test_unit_zip
  SOURCES ZipTest.cpp)

# The folder index relinking searches with.
score_add_test(test_unit_file_index
  SOURCES FileIndexTest.cpp
  PLUGINS score_lib_process)

# --- regression: per-app interface-list lookup in port deserialization -----
# Boots two headless apps in one process and round-trips ControlInlet /
# ControlOutlet through Process::load_inlet / load_outlet in each: a stale
# function-local static cache of the first app's PortFactoryList would be a
# heap-use-after-free in the second round. Needs the full app (APP) so the
# dynamic plugins that register the port factories (score_plugin_dataflow)
# are loaded from <build>/plugins.
score_add_test(test_unit_port_factory_reuse
  SOURCES PortFactoryListReuseTest.cpp
  APP
  PLUGINS score_lib_process)

# Graphics-view combo box: drop-down editor and free-value entry.
score_add_test(test_unit_graphics_combo
  SOURCES GraphicsComboTest.cpp
  APP)

if(TARGET score_plugin_controlsurface)
  # Control choice for a device parameter dropped on a Control surface.
  score_add_test(test_unit_controlsurface_control
    SOURCES ControlSurfaceControlTest.cpp
    APP
    PLUGINS score_plugin_controlsurface score_lib_process score_lib_device)
  target_include_directories(test_unit_controlsurface_control PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-controlsurface")
endif()

# Coverage of the add / edit address dialog's per-type forms.
score_add_test(test_unit_address_settings_widgets
  SOURCES AddressSettingsWidgetsTest.cpp
  APP
  PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)

# The shared per-type value editors used by the tree, the address panel and the
# address dialog.
score_add_test(test_unit_value_editors
  SOURCES ValueEditorsTest.cpp
  APP
  PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)

# Device explorer tree conversion: which ossia parameters become editable
# Device::Node values (regression #2204: audio gains).
score_add_test(test_unit_device_explorer_node
  SOURCES DeviceExplorerNodeTest.cpp
  PLUGINS score_lib_device)

# Add-device dialog: switching protocol must not leave an enumerator signal in
# flight pointing at a QTreeWidgetItem the switch deleted (use-after-free in
# QTreeWidgetItem::setExpanded reported from a release build).
score_add_test(test_unit_device_edit_dialog
  SOURCES DeviceEditDialogTest.cpp
  APP
  PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)

# The CAN protocol's device browser entry: the .dbc databases of the user
# library, plus the default interface against an independent walk of
# /sys/class/net.
if(TARGET score_plugin_protocols AND TARGET score_plugin_library)
  score_add_test(test_unit_can_enumerator
    SOURCES CANEnumeratorTest.cpp
    APP
    PLUGINS score_plugin_protocols score_plugin_library score_lib_device)
  target_include_directories(test_unit_can_enumerator PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-protocols")
  if(LINUX)
    target_compile_definitions(test_unit_can_enumerator PRIVATE OSSIA_PROTOCOL_CAN)
  endif()
endif()

# End-to-end CAN, driven through score's JavaScript scripting API (the `Score`
# object of the console panel): a device created from a script, real frames on
# vcan0, and the resulting node tree read back through iterateDevice.
# Skips itself when vcan0 is absent.
if(LINUX AND TARGET score_plugin_protocols AND TARGET score_plugin_js)
  score_add_test(test_unit_can_scripting
    SOURCES CANScriptingTest.cpp
    APP
    PLUGINS score_plugin_js score_plugin_protocols score_lib_device
    LIBS ossia ${QT_PREFIX}::Qml)
  target_include_directories(test_unit_can_scripting PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-js"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-protocols")
  target_compile_definitions(test_unit_can_scripting PRIVATE
    OSSIA_PROTOCOL_CAN
    SCORE_CAN_TEST_DATA="${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-protocols/Tests/data")
endif()

# The Mapper device driving libossia's QML protocol bindings: `Protocols` is a
# context property of the mapper's engine, and that engine runs on its own
# thread -- neither of which libossia's own QJSEngine-level tests can cover.
if(LINUX AND TARGET score_plugin_protocols AND TARGET score_plugin_js)
  score_add_test(test_unit_mapper_protocols
    SOURCES MapperProtocolsTest.cpp
    APP
    PLUGINS score_plugin_js score_plugin_protocols score_lib_device
    LIBS ossia ${QT_PREFIX}::Qml)
  target_include_directories(test_unit_mapper_protocols PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-js"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-protocols")
endif()

# --- core data model (score-lib-state / score-lib-process, P3R2) -----------
score_add_test(test_unit_state_serialization
  SOURCES StateSerializationTest.cpp
  PLUGINS score_lib_state)

# Port / Cable / ProcessModel object graph + DataStream & JSON round-trips.
score_add_test(test_unit_process_model
  SOURCES ProcessModelTest.cpp
  PLUGINS score_lib_process score_lib_state score_lib_device)

# Which ports a nodal-view node shows, and how it paginates its controls.
score_add_test(test_unit_port_visibility
  SOURCES PortVisibilityTest.cpp
  PLUGINS score_lib_process score_lib_state score_lib_device)

score_add_test(test_unit_curve_sample
  SOURCES CurveSampleTest.cpp
  PLUGINS score_plugin_curve)

score_add_test(test_unit_curve_roundtrip
  SOURCES CurveRoundtripTest.cpp
  APP
  PLUGINS score_plugin_curve score_lib_process)

score_add_test(test_unit_automation_values
  SOURCES AutomationValueTest.cpp
  APP
  PLUGINS score_plugin_automation score_plugin_curve score_lib_process)

if(TARGET score_plugin_mapping)
  score_add_test(test_unit_mapping_values
    SOURCES MappingTest.cpp
    APP
    PLUGINS score_plugin_mapping score_plugin_curve score_lib_process)
endif()

score_add_test(test_unit_spline_values
  SOURCES SplineValueTest.cpp
  PLUGINS score_plugin_spline)

# --- P3R2: score-plugin-midi + score-plugin-dataflow value tests -----------
if(TARGET score_plugin_midi)
  set(_midi_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-midi")
  score_plugin_hidden_sources(_midi_hidden
      "${_midi_src}/Midi/MidiNote.cpp"
      "${_midi_src}/Midi/MidiProcess.cpp"
      "${_midi_src}/Patternist/PatternParsing.cpp")
score_add_test(test_unit_midi_message
    SOURCES
      MidiMessageTest.cpp
      ${_midi_hidden}
    PLUGINS score_plugin_midi score_lib_process
    LIBS ${QT_PREFIX}::Gui)

  # Patternist's step engine, driven directly through pattern_node.
  score_add_test(test_unit_patternist_node
    SOURCES PatternistNodeTest.cpp
    PLUGINS score_plugin_midi score_lib_process
    LIBS ${QT_PREFIX}::Gui)
endif()

if(TARGET score_plugin_dataflow)
  score_add_test(test_unit_dataflow_value
    SOURCES DataflowValueTest.cpp
    PLUGINS score_plugin_dataflow score_lib_process)

endif()


# --- analysis DSP ------------------------------------------------------------
if(TARGET score_plugin_analysis)
  set(_gist_dir "${3RDPARTY_FOLDER}/Gist/src")
  score_plugin_hidden_sources(_gist_hidden
      "${_gist_dir}/CoreFrequencyDomainFeatures.cpp"
      "${_gist_dir}/CoreTimeDomainFeatures.cpp"
      "${_gist_dir}/Gist.cpp"
      "${_gist_dir}/MFCC.cpp"
      "${_gist_dir}/OnsetDetectionFunction.cpp"
      "${_gist_dir}/WindowFunctions.cpp"
      "${_gist_dir}/Yin.cpp")
score_add_test(test_unit_analysis_gist
    SOURCES
      AnalysisGistTest.cpp
      ${_gist_hidden}
    PLUGINS score_plugin_analysis score_plugin_avnd)
  target_compile_definitions(test_unit_analysis_gist PRIVATE USE_OSSIA_FFT=1)
  target_include_directories(test_unit_analysis_gist SYSTEM PRIVATE "${_gist_dir}")
  target_include_directories(test_unit_analysis_gist PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-analysis")
endif()

# --- faust JIT DSP -----------------------------------------------------------
if(TARGET score_plugin_faust)
  score_add_test(test_unit_faust_dsp
    SOURCES FaustDspTest.cpp
    PLUGINS score_plugin_faust)
  target_include_directories(test_unit_faust_dsp PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-faust")
  # score_plugin_faust keeps FAUST_INCLUDE_DIR PRIVATE, so linking it does not
  # bring the faust headers along; FaustDspTest.cpp reaches libossia headers
  # that include <faust/gui/UI.h> directly. The find_package branch below
  # already does this.
  target_include_directories(test_unit_faust_dsp SYSTEM PRIVATE
    ${FAUST_INCLUDE_DIR})
else()
  find_package(Faust QUIET)
  if(FAUST_FOUND AND TARGET score_lib_process)
    score_add_test(test_unit_faust_dsp
      SOURCES FaustDspTest.cpp
      PLUGINS score_lib_process
      LIBS ${FAUST_LIBRARIES})
    target_compile_definitions(test_unit_faust_dsp PRIVATE FAUSTFLOAT=double)
    target_include_directories(test_unit_faust_dsp PRIVATE
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-faust")
    target_include_directories(test_unit_faust_dsp SYSTEM PRIVATE
      ${FAUST_INCLUDE_DIR})
  endif()
endif()

# --- cross-implementation quantification grid parity ------------------------
# ossia::token_request vs halp::tick_musical: a native node and an avendish
# plug-in on the same score must snap to the same samples.
score_add_test(test_unit_quantification_parity
  SOURCES QuantificationParityTest.cpp
  PLUGINS score_lib_process)
target_include_directories(test_unit_quantification_parity SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include")

# --- P3R3: audio DSP value-asserting tests ---------------------------------
score_add_test(test_unit_avnd_audio_effects
  SOURCES AvndAudioEffectsTest.cpp)
target_include_directories(test_unit_avnd_audio_effects SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

score_add_test(test_unit_avnd_midi_envelope
  SOURCES AvndMidiEnvelopeTest.cpp)
target_include_directories(test_unit_avnd_midi_envelope SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

score_add_test(test_unit_avnd_midi_humanizer
  SOURCES AvndMidiHumanizerTest.cpp)
target_include_directories(test_unit_avnd_midi_humanizer SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

if(TARGET score_plugin_fx)
  score_add_test(test_unit_fx_audio_dsp
    SOURCES FxAudioDspTest.cpp
    PLUGINS score_plugin_fx)

  # The value-domain ExprTK processes (Micromap, Expression Value Filter /
  # Generator, Arraymap, Arraygen) driven through GenericMathMapping, with the
  # expressions that actually ship in packages/default/Presets.
  score_add_test(test_unit_fx_expression
    SOURCES FxExpressionTest.cpp
    PLUGINS score_plugin_fx)

  # Conformance sweep over every ExprTK preset shipped in the score user
  # library: each one must compile, keep the shape its node promises, and never
  # produce a NaN or an infinity. The corpus header is generated from the .scp
  # files by tools/gen-expression-preset-corpus.js.
  score_add_test(test_unit_expression_presets
    SOURCES ExpressionPresetsTest.cpp
    PLUGINS score_plugin_fx)
endif()

# --- ExprTK wrapper ----------------------------------------------------------
# ossia::math_expression itself: the `return [...]` results protocol, the
# lexical has_variable() query the nodes switch on, ExprTK vector views and the
# score-specific function library.
score_add_test(test_unit_math_expression
  SOURCES MathExpressionTest.cpp
  LIBS ossia)

score_add_test(test_unit_audio_gain_pan
  SOURCES AudioGainPanTest.cpp)

# --- graphics control dragging (pointer lock / infinite scroller) -----------
# Drives InfiniteScroller against a fake score::PointerLock. InfiniteScroller.cpp
# is compiled into the test so that its calls bind to the fake rather than to the
# platform backend in score_lib_base.
score_add_test(test_unit_infinite_scroller
  STANDALONE
  SOURCES
    InfiniteScrollerTest.cpp
    "${SCORE_ROOT_SOURCE_DIR}/src/lib/score/graphics/InfiniteScroller.cpp"
  LIBS
    ${QT_PREFIX}::Gui
    ${QT_PREFIX}::GuiPrivate
    ${QT_PREFIX}::Widgets)
target_compile_definitions(test_unit_infinite_scroller
  PRIVATE SCORE_LIB_BASE_STATIC_DEFINE)

# Drives the note chooser through its own mouse handlers with the real
# InfiniteScroller: on the offscreen platform no pointer lock is granted, so the
# drag goes through the pointer positions.
score_add_test(test_unit_note_chooser
  SOURCES NoteChooserTest.cpp
  APP)

# The spinbox maps the drag itself instead of going through
# InfiniteScroller::move(); APP because its header pulls in the skin.
score_add_test(test_unit_spinbox_drag
  SOURCES SpinboxDragTest.cpp
  APP)

score_add_test(test_unit_avnd_audio_roundtrip
  SOURCES AvndAudioRoundtripTest.cpp
  PLUGINS score_lib_process
  APP)

# --- LV2 loading & processing, against the fixture bundle -------------------
if(TARGET score_plugin_lv2)
  score_add_test(test_unit_lv2_loading
    SOURCES LV2LoadingTest.cpp
    APP
    PLUGINS score_plugin_lv2 score_lib_process
    LIBS Lilv)
  add_dependencies(test_unit_lv2_loading score_test_gain_lv2)
  target_compile_definitions(test_unit_lv2_loading PRIVATE
    SCORE_TEST_LV2_BUNDLE="${SCORE_ROOT_BINARY_DIR}/tests/lv2/score-test-gain.lv2"
    SCORE_TEST_QT6_ELF="$<TARGET_FILE:score_plugin_lv2>")
  target_include_directories(test_unit_lv2_loading PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-lv2"
    "${LV2_PATH}"
    $<TARGET_PROPERTY:Suil,INTERFACE_INCLUDE_DIRECTORIES>
    "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
endif()

# An item deleted from inside its own event delivery corrupts QGraphicsScene's
# mouse-grabber stack; deleteGraphicsItem/Object defer past the delivery.
score_add_test(test_unit_graphics_item_delete
  SOURCES GraphicsItemDeleteTest.cpp
  APP)

# --- PipeWire quantum handling ---------------------------------------------
# Only built where pipewire headers are available (pipewire::pipewire is
# created by libossia's deps when they are found).
if(TARGET pipewire::pipewire)
  # The graph quantum is a global negotiation the engine does not control
  # (competing node.force-quantum clients, global clock.force-quantum,
  # quantum-floor/limit clamping). Pure cycle policy: chunk splitting,
  # once-per-reconfiguration logging, scratch-pointer assembly.
  score_add_test(test_unit_pipewire_quantum
    SOURCES PipewireQuantumTest.cpp)

  # End-to-end against a private pipewire daemon spawned in a scratch
  # PIPEWIRE_RUNTIME_DIR (skips when no daemon can be started): the graph
  # is pinned to a quantum the engine did not ask for and the engine must
  # keep ticking in slices of its configured block size instead of going
  # silent.
  score_add_test(test_unit_pipewire_protocol_quantum
    SOURCES PipewireProtocolQuantumTest.cpp)
  get_target_property(_pw_includes pipewire::pipewire INTERFACE_INCLUDE_DIRECTORIES)
  if(_pw_includes)
    target_include_directories(test_unit_pipewire_protocol_quantum SYSTEM PRIVATE ${_pw_includes})
  endif()
endif()

# --- Plug-in scanner ("puppet") protocol helpers ----------------------------
# Unescaped plug-in metadata used to break entire scan replies; the request
# id / port / token argv protocol routes replies to the right scan session.
score_add_test(test_unit_puppet_json
  SOURCES PuppetJsonTest.cpp)

# Versioned scan-cache blob + healing of caches polluted by the fixed-port
# cross-instance duplication bug (200+ copies per plug-in were observed).
score_add_test(test_unit_audio_plugin_cache
  SOURCES AudioPluginCacheTest.cpp
  PLUGINS score_plugin_media)

# --- optional dynamic dependencies ------------------------------------------
# A library that is not installed must leave its loader unavailable instead of
# throwing out of instance().
score_add_test(test_unit_dynamic_library
  SOURCES DynamicLibraryTest.cpp)

# --- video input timeouts ---------------------------------------------------
# Blocking libav I/O on a device or stream that stops answering: the deadline
# libav polls, and the abort that lets close_file() join its buffer thread.
# The two end-to-end cases drive CameraInput / LibavStreamInput against a local
# server that accepts and then stays silent, which used to block forever.
if(TARGET score_plugin_media)
  score_add_test(test_unit_libav_interrupt
    SOURCES LibavInterruptTest.cpp
    PLUGINS score_plugin_media
    LIBS ${QT_PREFIX}::Network)
  target_include_directories(test_unit_libav_interrupt PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-media")
endif()

# The shared scan state machine, end-to-end against a scriptable fake puppet:
# session-token isolation, crash/timeout single-failure, reply-vs-exit races,
# clean cancellation of a scan in progress.
if(SCORE_HAS_FAKE_PUPPET)
  score_add_test(test_unit_plugin_scanner
    SOURCES PluginScannerTest.cpp
    PLUGINS score_plugin_media)
  add_dependencies(test_unit_plugin_scanner score_test_fake_puppet)
  target_compile_definitions(test_unit_plugin_scanner PRIVATE
    SCORE_FAKE_PUPPET="$<TARGET_FILE:score_test_fake_puppet>")
endif()

# Per-backend scan-reply parsing + cache healing.
if(TARGET score_plugin_vst)
  score_add_test(test_unit_vst_scan_reply
    SOURCES VstScanReplyTest.cpp
    PLUGINS score_plugin_vst)
  target_include_directories(test_unit_vst_scan_reply PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst")
endif()

if(TARGET score_plugin_vst3)
  score_add_test(test_unit_vst3_scan_reply
    SOURCES Vst3ScanReplyTest.cpp
    PLUGINS score_plugin_vst3
    LIBS sdk_common sdk_hosting)
  target_include_directories(test_unit_vst3_scan_reply PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst3")
endif()

if(TARGET score_plugin_clap)
  # The healing case boots the app: a legacy cache polluted with hundreds of
  # cross-instance duplicates must come out deduplicated and migrated to the
  # versioned format.
  score_add_test(test_unit_clap_scan
    SOURCES ClapScanTest.cpp
    APP
    PLUGINS score_plugin_clap)
  target_include_directories(test_unit_clap_scan PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-clap")

  # The tick -> clap_event_transport_t conversion (fixed-point song positions;
  # transport-following plug-ins froze at position 0 when this was wrong).
  score_add_test(test_unit_clap_transport
    SOURCES ClapTransportTest.cpp
    PLUGINS score_lib_process)
  target_include_directories(test_unit_clap_transport PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-clap"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/clap/include")
endif()

# Per-format plug-in path settings; VST3 must react to Vst3Paths, not VstPaths.
if(TARGET score_plugin_vst3 AND TARGET score_plugin_clap)
  score_add_test(test_unit_plugin_path_settings
    SOURCES PluginPathSettingsTest.cpp
    APP
    PLUGINS score_plugin_vst3 score_plugin_clap
    LIBS sdk_common sdk_hosting)
  target_include_directories(test_unit_plugin_path_settings PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-vst3"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-clap")
endif()

# --- beat tracker DSP -------------------------------------------------------
# The ODF's fixed internal hop (independence from the host block size), onset
# timing, tempo lock, the DLL's convergence/monotonicity/continuity contract,
# the error pre-filter, hold/re-lock across silence, and tap tempo semantics.
score_add_test(test_unit_beat_tracker
  SOURCES BeatTrackerTest.cpp
  LIBS ossia)
target_include_directories(test_unit_beat_tracker SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include")
target_include_directories(test_unit_beat_tracker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd")

# Dragging a cable against the edge of the view scrolls it.
score_add_test(test_unit_cable_drag_autoscroll
  SOURCES CableDragAutoScrollTest.cpp
  APP
  PLUGINS score_lib_process)

# Multi-line "pretty" rendering of nested values (Value display process).
score_add_test(test_unit_value_pretty_print
  SOURCES ValuePrettyPrintTest.cpp
  PLUGINS score_lib_state)

# When the explorer re-explores a device's namespace (add, edit, undo of a
# removal), and which nodes survive it.
score_add_test(test_unit_device_tree_reload
  SOURCES DeviceTreeReloadTest.cpp
  APP
  PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)

# The tracking protocols addon as devices: every protocol can be added to a
# document and coexists with the others (what the "Add device" dialog checks).
if(TARGET score_addon_trackingprotocols)
  score_add_test(test_tracking_devices
    SOURCES TrackingDevicesTest.cpp
    APP
    PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)
endif()

# The explorer listens to exactly what it shows, across device tree swaps and
# settings edits. GUI: needs the device explorer panel and its view.
score_add_test(test_unit_device_listening
  SOURCES DeviceListeningTest.cpp
  GUI
  PLUGINS score_plugin_deviceexplorer score_lib_device score_lib_state)

# The audio engine across documents being opened, switched and closed: it
# lives as long as a document does, whichever document closes.
if(TARGET score_plugin_audio)
  score_add_test(test_unit_audio_engine_documents
    SOURCES AudioEngineDocumentsTest.cpp
    APP
    PLUGINS score_plugin_audio score_plugin_deviceexplorer score_lib_device)
endif()

# The delayed reset after a stop acts on the interval it was armed for, not
# on whatever document is current (or closing) when its timer fires.
if(TARGET score_plugin_engine)
  score_add_test(test_unit_execution_reset_after_close
    SOURCES ExecutionResetAfterCloseTest.cpp
    APP
    PLUGINS score_plugin_engine score_plugin_scenario)
endif()
# The addresses a port's combo box lists: which devices are walked (their
# declared node kinds) and which nodes qualify for each port type / direction.
score_add_test(test_unit_port_address_list
  SOURCES PortAddressListTest.cpp
  PLUGINS score_lib_process score_lib_device score_lib_state)

# What dropping device explorer nodes (a parameter, a device root) on a port
# or an address field resolves to.
score_add_test(test_unit_port_drop
  SOURCES PortDropTest.cpp
  PLUGINS score_lib_device score_lib_state)

# A device's node callbacks across disconnect / reconnect: disconnecting must
# not report the nodes as removed, reconnecting must watch the new tree.
score_add_test(test_unit_device_disconnect_callbacks
  SOURCES DeviceDisconnectCallbacksTest.cpp
  PLUGINS score_lib_device score_lib_state)

  # Where the file / folder pickers open: next to the current file, else the
# project, the library, the user's documents, the working directory.
score_add_test(test_unit_picker_start_folder
  SOURCES PickerStartFolderTest.cpp)



# The GStreamer input device's two pieces of pure logic: appsink classification
# from the pipeline string (no negotiated caps on a live source) and the audio
# ring buffer whose per-tick resize must re-point the engine's spans. Neither
# needs a GStreamer library.
if(TARGET score_plugin_gfx AND TARGET score_plugin_media)
  score_add_test(test_unit_gstreamer_pipeline_parse
    SOURCES GStreamerPipelineParseTest.cpp
    PLUGINS score_plugin_gfx score_plugin_media
    LIBS avutil)

  score_add_test(test_unit_gstreamer_audio_buffer
    SOURCES GStreamerAudioBufferTest.cpp
    PLUGINS score_plugin_gfx)
endif()

# Video::gstreamerToLibav(): the gst-name -> AVPixelFormat table feeding the
# GStreamer, Sh4lt and Shmdata inputs. Header-only and self-contained, so the
# test needs the plug-in only for its libav include paths and SCORE_HAS_LIBAV.
if(TARGET score_plugin_media)
  score_add_test(test_unit_gstreamer_compat
    SOURCES GStreamerCompatibilityTest.cpp
    PLUGINS score_plugin_media
    LIBS avutil)

  # LibAVDecoder::enqueue_frame(): the avcodec_send_packet /
  # avcodec_receive_frame loop behind every video decode path. Synthesizes
  # encoded streams in-process, drives the codec into the send-EAGAIN
  # backpressure state, and asserts the N-in / N-out / in-order invariant —
  # including the EOF reorder-buffer flush through the full VideoDecoder.
  score_add_test(test_unit_video_decoder
    SOURCES VideoDecoderTest.cpp
    PLUGINS score_plugin_media
    LIBS avutil avcodec avformat)
endif()

# The video pixel-format vocabulary and its libav bridge. Lives with the
# vocabulary rather than with its consumers, so the PR that introduces a format
# is the one whose CI proves it is described and sized correctly.
score_add_test(test_unit_video_pixel_format
  SOURCES VideoPixelFormatTest.cpp
  PLUGINS score_plugin_gfx
  LIBS avutil)

# The YCbCr->RGB matrices and the bit-depth normalisation constants in
# ColorSpace.hpp. Half of it derives every coefficient from Kr/Kb and needs
# nothing; the other half generates clips with ffmpeg and replays the shader
# arithmetic on them, and skips when ffmpeg is not on PATH.
if(TARGET score_plugin_media)
  score_add_test(test_unit_video_colorspace
    SOURCES VideoColorSpaceTest.cpp
    PLUGINS score_plugin_gfx score_plugin_media
    LIBS avutil)
endif()

# The DirectShow enumeration decision -- subtype -> fourcc -> layout -> codec.
# Deliberately NOT WIN32-guarded: the whole point of DirectShowSubtype.hpp is
# that the decision is checkable on hosts where CameraDevice.win32.cpp cannot
# be compiled at all.
score_add_test(test_unit_dshow_subtype
  SOURCES DirectShowSubtypeResolveTest.cpp
  PLUGINS score_plugin_gfx
  LIBS avutil avcodec)
# AssetTable is pure CPU logic; TextureLoader upload paths run on QRhi's Null
# backend (score_plugin_gfx links Qt::GuiPrivate PUBLIC, providing qrhi_p.h).
# Lives with the scene rework rather than with the AssetTable code itself: the
# trim and decode-cache behaviour it pins is what this commit introduces, so
# committed any earlier the suite is red for every commit in between.
score_add_test(test_unit_gfx_assettable
  SOURCES AssetTableTest.cpp
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
# libisf importer branches (shadertoy / shadertoy-json / glsl-sandbox /
# write_isf / float-input min-max inference). isf::parser is exported from
# score_plugin_gfx, whose PUBLIC include dirs provide <isf.hpp>.
score_add_test(test_unit_isf_importers
  SOURCES IsfImportersTest.cpp
  PLUGINS score_plugin_gfx)


# --- CPU scene flattener + camera math (score-plugin-gfx, pland/scene) ------
# flattenScene / packMaterial / primitiveToGeometry / packCameraUBO are not
# exported from the dynamic score_plugin_gfx (hidden visibility), so the two
# translation units are compiled straight into the tests -- the same workaround
# tests/gfx/CMakeLists.txt applies to ISFNode.cpp et al. Pure CPU: no QRhi, no
# display, no document.
if(TARGET score_plugin_gfx)
  set(_gfx_scene_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx/Gfx/Graph")

  score_plugin_hidden_sources(_scene_flatten_hidden
      "${_gfx_scene_src}/SceneGPUState.cpp")
score_add_test(test_unit_scene_flatten
    SOURCES
      SceneFlattenTest.cpp
      ${_scene_flatten_hidden}
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)

  score_plugin_hidden_sources(_camera_math_hidden
      "${_gfx_scene_src}/CameraMath.cpp")
score_add_test(test_unit_camera_math
    SOURCES
      CameraMathTest.cpp
      ${_camera_math_hidden}
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)

  # ISOLATED, EXPECTED-RED (#163): the Camera auxiliary declares one 240-byte
  # entry however many cameras the flattener packed into the buffer. Its own
  # target, like test_gfx_isf_findings, so the attributable failure cannot take
  # down the flattener suite.
  score_plugin_hidden_sources(_camera_aux_hidden
      "${_gfx_scene_src}/SceneGPUState.cpp")
score_add_test(test_unit_scene_camera_aux
    SOURCES
      SceneCameraAuxTest.cpp
      ${_camera_aux_hidden}
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)

  # ISOLATED, EXPECTED-RED (#172). PipelineStateHelpers is
  # exported, so only the test TU is built here; it needs qrhi_p.h, which
  # score_plugin_gfx provides through its PUBLIC Qt::GuiPrivate link.
  score_add_test(test_unit_depth_clear_compare
    SOURCES DepthClearCompareTest.cpp
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)

  # bindUpstreamBuffers' uniform_input usage check. bindUpstreamBuffers and
  # RenderList are exported, so only the test TU is built; the QRhi it needs
  # comes from score_plugin_gfx's PUBLIC Qt::GuiPrivate link. Runs on the Null
  # backend on purpose -- the assertion is a CPU binding decision.
  score_add_test(test_unit_isf_uniform_input_usage
    SOURCES IsfUniformInputUsageTest.cpp
    PLUGINS score_plugin_gfx score_lib_process
    LIBS ${QT_PREFIX}::Gui)

  # The output renderers' null-pipeline guard. ScaledRenderer is not exported
  # from the dynamic plugin, so InvertYRenderer.cpp -- which defines both it and
  # InvertYRenderer -- is compiled into the test. ISOLATED: the defect aborts
  # the process rather than failing an assertion, which would take any
  # co-located test case down with it.
  score_plugin_hidden_sources(_null_pipeline_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx/Gfx/InvertYRenderer.cpp")
  score_add_test(test_unit_output_null_pipeline
    SOURCES
      OutputNullPipelineTest.cpp
      ${_null_pipeline_hidden}
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)
endif()

# The DRM fourcc <-> AVPixelFormat bridge layered on the vocabulary
# (Gfx/Graph/interop/DrmFourcc.hpp), which is a different header from the
# DrmPixelFormat.hpp that VideoPixelFormatTest sweeps.
if(TARGET score_plugin_gfx)
  score_add_test(test_unit_drm_fourcc
    SOURCES DrmFourccTest.cpp
    PLUGINS score_plugin_gfx
    LIBS avutil)

  # The capture-side mirror of the WireEncoderFactory sweep in
  # tests/integration/EncoderMatrixTest.cpp. Constructs decoders only: no QRhi.
  score_add_test(test_unit_wire_decoder_factory
    SOURCES WireDecoderFactoryTest.cpp
    PLUGINS score_plugin_gfx score_plugin_media
    LIBS ${QT_PREFIX}::Gui)

  # PipeWire's own format table, and the SPA half of DrmFourcc.hpp, which only
  # compiles where the SPA headers are.
  if(TARGET pipewire::pipewire)
    score_add_test(test_unit_pipewire_formats
      SOURCES PipewireFormatsTest.cpp
      PLUGINS score_plugin_gfx
      LIBS avutil ${QT_PREFIX}::Gui)
    get_target_property(_pw_fmt_includes pipewire::pipewire INTERFACE_INCLUDE_DIRECTORIES)
    if(_pw_fmt_includes)
      target_include_directories(test_unit_pipewire_formats SYSTEM PRIVATE ${_pw_fmt_includes})
    endif()

    # The process-wide libremidi pipewire connection is shared by the gfx
    # output producer, the gfx input consumer, the audio engine and the MIDI
    # backends. Reconnecting it destroys the pw_core / pw_thread_loop every
    # other holder's pw_stream was built on. Drives a private daemon.
    score_add_test(test_unit_pipewire_shared_context
      SOURCES PipewireSharedContextTest.cpp
      PLUGINS score_plugin_gfx
      LIBS ${QT_PREFIX}::Gui)
    get_target_property(_pw_ctx_includes pipewire::pipewire INTERFACE_INCLUDE_DIRECTORIES)
    if(_pw_ctx_includes)
      target_include_directories(test_unit_pipewire_shared_context SYSTEM PRIVATE ${_pw_ctx_includes})
    endif()
  endif()
endif()

# --- image loaders (gfx TextureLoader decode + threedim LDR ImageLoader /
# ArrayToTexture) ----------------------------------------------------------
# All entry points are either SCORE_PLUGIN_GFX_EXPORTed (decodeImageFrom*)
# or header-inline (Threedim::ImageLoader::ins::image_t::process,
# ArrayToTexture::recreate), so no plugin .cpp needs to be recompiled here.
if(TARGET score_plugin_threedim)
  score_add_test(test_unit_image_loader
    SOURCES ImageLoaderTest.cpp
    PLUGINS score_plugin_threedim score_plugin_gfx score_plugin_avnd
    LIBS ${QT_PREFIX}::Gui)
endif()

# --- RDMA playout probe (score-plugin-gfx) ----------------------------------
# The gate that decides whether a GPU-direct output rung may engage. The vendor
# DMA calls are injected, so the whole decision table -- including the peer that
# returns a constant while every return code says success -- runs with no card
# and no CUDA driver.
if(TARGET score_plugin_gfx)
  score_add_test(test_unit_rdma_playout_probe
    SOURCES RdmaPlayoutProbeTest.cpp
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)
endif()
