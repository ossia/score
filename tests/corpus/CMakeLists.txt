# Corpus-scale video decoding validation. Not a ctest: the tester takes one
# media file per invocation and run-corpus.sh drives it over a whole library
# (FATE suite, generated matrices) with per-file process isolation, so a
# crashing sample is a report line instead of a dead test run.
if(TARGET score_plugin_media)
  add_executable(score_video_corpus_tester VideoCorpusTester.cpp)
  target_link_libraries(score_video_corpus_tester PRIVATE
    score_lib_base
    score_plugin_media
    avcodec avformat avutil swscale
    ${QT_PREFIX}::Core)
  setup_score_common_exe_features(score_video_corpus_tester)
  # Both testers are raw add_executable rather than score_add_test, so they miss
  # the enable_minimal_qt_plugins() that ScoreTests.cmake:157 gives every other
  # test. On a STATIC Qt -- which is what the Windows and macOS SDKs ship -- no
  # platform plugin is linked in and the binary cannot start at all. The
  # function early-returns on a dynamic Qt, so this is free on Linux.
  # QCoreApplication only here: no GUI.
  enable_minimal_qt_plugins(score_video_corpus_tester 0)
  set_target_properties(score_video_corpus_tester PROPERTIES FOLDER "Tests")
endif()

# Corpus-scale *document* validation: load / round-trip / build the execution
# graph / render one frame, for real user .score files. Same shape as the
# video tester above and for the same reason -- one document per invocation,
# with run-score-corpus.sh driving a whole directory and turning a crash or a
# hang into a report line instead of a dead run. Not a ctest: the corpus lives
# outside the repository (SCORE_CORPUS_DIR, default $HOME/ossia/score-corpus)
# and must never be committed.
if(TARGET score_plugin_gfx AND TARGET score_plugin_engine AND TARGET score_plugin_scenario)
  add_executable(score_corpus_tester ScoreCorpusTester.cpp)
  target_link_libraries(score_corpus_tester PRIVATE
    score_lib_base
    score_plugin_scenario score_plugin_engine score_plugin_gfx
    score_plugin_deviceexplorer score_lib_process score_lib_device
    ${QT_PREFIX}::Core ${QT_PREFIX}::Gui ${QT_PREFIX}::Widgets
    ${QT_PREFIX}::Network ${QT_PREFIX}::Xml)
  target_compile_definitions(score_corpus_tester PRIVATE
    "SCORE_TEST_BINARY_DIR=\"${SCORE_ROOT_BINARY_DIR}\"")
  setup_score_common_exe_features(score_corpus_tester)
  # See the note on the video tester above. This one builds a QApplication and
  # renders a frame, so it needs the GUI set (offscreen included).
  enable_minimal_qt_plugins(score_corpus_tester 1)
  set_target_properties(score_corpus_tester PROPERTIES FOLDER "Tests")
endif()
