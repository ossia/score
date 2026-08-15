# Tests for the process-library model population (staged subtree publish).
# APP tests: they need the full headless application so that the settings
# models and the process factories from every plugin are registered.
# QtTest is linked for QAbstractItemModelTester.
find_package(${QT_VERSION} REQUIRED COMPONENTS Test)

score_add_test(test_library_publish
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ProcessLibraryPublishTest.cpp"
  APP
  PLUGINS score_plugin_library score_lib_process
  LIBS ${QT_PREFIX}::Test)
