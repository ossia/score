# Hermetic LV2 bundle for the LV2 loading/processing tests: a tiny gain
# plug-in compiled at build time into <build>/tests/lv2/score-test-gain.lv2.
# Only built when score_plugin_lv2 itself is (needs the LV2 headers).

if(NOT TARGET score_plugin_lv2)
  return()
endif()

set(_bundle_dir "${SCORE_ROOT_BINARY_DIR}/tests/lv2/score-test-gain.lv2")

add_library(score_test_gain_lv2 MODULE score_test_gain.c)
set_target_properties(score_test_gain_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "score_test_gain"
  LIBRARY_OUTPUT_DIRECTORY "${_bundle_dir}"
  # ScoreTests.cmake test targets don't go through the plugin install steps
  FOLDER "Tests")
target_include_directories(score_test_gain_lv2 PRIVATE "${LV2_PATH}")

if(WIN32)
  set(LV2_TEST_BINARY_NAME "score_test_gain.dll")
else()
  set_target_properties(score_test_gain_lv2 PROPERTIES SUFFIX ".so")
  set(LV2_TEST_BINARY_NAME "score_test_gain.so")
endif()

configure_file(manifest.ttl.in "${_bundle_dir}/manifest.ttl" @ONLY)
configure_file(score_test_gain.ttl "${_bundle_dir}/score_test_gain.ttl" COPYONLY)
