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
  set_target_properties(score_video_corpus_tester PROPERTIES FOLDER "Tests")
endif()
