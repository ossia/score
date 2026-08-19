# score_test_fixtures: the shared test-support layer (headless app bootstrap,
# document builders, serialization helpers). Header-only for now.
add_library(score_test_fixtures INTERFACE)

target_include_directories(score_test_fixtures
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(score_test_fixtures
  INTERFACE score_lib_base ${QT_PREFIX}::Core)

# Test LV2 bundle (only when score_plugin_lv2 is built).
add_subdirectory(lv2)

# Scriptable stand-in for the plug-in scanner puppets (PluginScanner tests).
#
# Only with a static Qt: the source takes the address of a QWebSocket signal in a
# constexpr initialiser, and against a shared Qt that address comes from an import
# thunk resolved at load time, so it is not a constant expression.
get_target_property(_score_qt_core_type ${QT_PREFIX}::Core TYPE)
if(_score_qt_core_type STREQUAL "STATIC_LIBRARY")
  set(SCORE_HAS_FAKE_PUPPET 1 CACHE INTERNAL "")
  add_executable(score_test_fake_puppet fake_puppet/fake_puppet.cpp)
  target_link_libraries(score_test_fake_puppet PRIVATE
    ${QT_PREFIX}::Core ${QT_PREFIX}::WebSockets)
  set_target_properties(score_test_fake_puppet PROPERTIES
    FOLDER "Tests"
    SCORE_CUSTOM_PCH 1
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
else()
  set(SCORE_HAS_FAKE_PUPPET 0 CACHE INTERNAL "")
  message(STATUS "score: fake puppet skipped (Qt is not static)")
endif()

# Make the headers visible in IDEs.
file(GLOB_RECURSE _fixture_hdrs "${CMAKE_CURRENT_SOURCE_DIR}/score_test/*.hpp")
add_custom_target(score_test_fixtures_headers SOURCES ${_fixture_hdrs})
set_target_properties(score_test_fixtures_headers PROPERTIES FOLDER "Tests")
