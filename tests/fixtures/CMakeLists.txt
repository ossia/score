# score_test_fixtures: the shared test-support layer (headless app bootstrap,
# document builders, serialization helpers). Header-only for now.
add_library(score_test_fixtures INTERFACE)

target_include_directories(score_test_fixtures
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(score_test_fixtures
  INTERFACE score_lib_base ${QT_PREFIX}::Core)

# Make the headers visible in IDEs.
file(GLOB_RECURSE _fixture_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/score_test/*.hpp")
add_custom_target(score_test_fixtures_headers SOURCES ${_fixture_hdrs})
set_target_properties(score_test_fixtures_headers PROPERTIES FOLDER "Tests")
