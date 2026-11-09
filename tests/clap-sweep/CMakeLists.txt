# Every CLAP plug-in installed on the machine, run through score's host and a
# minimal reference one; see clap_sweep.py. Machine-dependent and long: the
# ctest entry is skipped unless SCORE_CLAP_SWEEP=1.

# The checker and the worker's reference host load plug-ins with dlopen
if(NOT TARGET score_plugin_clap OR WIN32)
  return()
endif()

# The plug-in both hosts load the swept ones through: it forwards everything
# and reports what the host did that the specification forbids. Never
# unloaded, as it keeps the swept plug-in loaded.
add_library(clap_checker MODULE ClapChecker.cpp)
target_include_directories(clap_checker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/clap/include")
target_compile_features(clap_checker PRIVATE cxx_std_20)
target_link_libraries(clap_checker PRIVATE ${CMAKE_DL_LIBS})
if(NOT APPLE)
  target_link_options(clap_checker PRIVATE "-Wl,-z,nodelete")
endif()
set_target_properties(clap_checker PROPERTIES PREFIX "" SUFFIX ".clap")

score_add_test(test_clap_sweep_worker
  SOURCES ClapSweepWorker.cpp
  APP
  NO_CTEST
  PLUGINS score_plugin_clap score_plugin_audio score_plugin_engine score_plugin_scenario score_lib_process
  LIBS ${CMAKE_DL_LIBS})
target_include_directories(test_clap_sweep_worker PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-clap"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/clap/include")
add_dependencies(test_clap_sweep_worker clap_checker)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND SCORE_HAS_SHELL_HARNESS)
  add_test(NAME test_clap_system_sweep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/clap_sweep.py"
      --worker "$<TARGET_FILE:test_clap_sweep_worker>"
      --checker "$<TARGET_FILE:clap_checker>"
      --build "${SCORE_ROOT_BINARY_DIR}"
      --out "${SCORE_ROOT_BINARY_DIR}/tests/clap-sweep"
      --require-opt-in
    WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
  set_tests_properties(test_clap_system_sweep PROPERTIES
    LABELS "clap;sweep"
    SKIP_RETURN_CODE 77
    TIMEOUT 7200)
endif()
