# ossia score — testing infrastructure.
#
# Single entry point for declaring tests. Active only when SCORE_TESTING is ON.
#
# Usage:
#   score_add_test(my_test
#     SOURCES   Foo.cpp Bar.cpp
#     PLUGINS   score_lib_state          # extra score libs/plugins to link
#     APP                                # needs the full headless app (run from build root)
#     GUI                                # needs a GUI QApplication (links Qt Widgets/Gui)
#     LIBS      some_other_lib)          # arbitrary extra link libraries
#
# A plain test links score_lib_base + score_test_fixtures + Catch2 and runs as a
# single ctest entry. An APP/GUI test additionally runs from the build root so
# that the dynamic plugins in <build>/plugins are discovered at runtime
# (see score::PluginLoader::pluginsDir(), which probes "<cwd>/plugins").

include_guard(GLOBAL)

# Make the Catch2 target (Catch2::Catch2WithMain) available. Catch2 is vendored
# inside libossia's 3rdparty tree; we add it ourselves (rather than relying on
# OSSIA_TESTING, which would also drag in libossia's own test tree).
function(score_setup_catch2)
  if(TARGET Catch2::Catch2WithMain)
    return()
  endif()

  set(_catch2_dir "${SCORE_ROOT_SOURCE_DIR}/3rdparty/libossia/3rdparty/Catch2")
  if(NOT EXISTS "${_catch2_dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "SCORE_TESTING is ON but Catch2 was not found at:\n  ${_catch2_dir}\n"
      "Make sure git submodules are initialized.")
  endif()

  set(CATCH_INSTALL_DOCS 0 CACHE INTERNAL "" FORCE)
  set(CATCH_INSTALL_EXTRAS 0 CACHE INTERNAL "" FORCE)
  set(CATCH_BUILD_TESTING 0 CACHE INTERNAL "" FORCE)
  set(CATCH_BUILD_STATIC_LIBRARY 1 CACHE INTERNAL "" FORCE)

  set(_old_shared "${BUILD_SHARED_LIBS}")
  set(BUILD_SHARED_LIBS 0)
  add_subdirectory("${_catch2_dir}" "${SCORE_ROOT_BINARY_DIR}/3rdparty/Catch2" EXCLUDE_FROM_ALL)
  set(BUILD_SHARED_LIBS "${_old_shared}")
endfunction()

function(score_add_test NAME)
  cmake_parse_arguments(ARG "GUI;APP" "" "SOURCES;PLUGINS;LIBS" ${ARGN})

  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "score_add_test(${NAME}): no SOURCES given")
  endif()

  add_executable(${NAME} ${ARG_SOURCES})

  target_link_libraries(${NAME} PRIVATE
    score_lib_base
    Catch2::Catch2WithMain
    ${ARG_PLUGINS}
    ${ARG_LIBS}
    ${QT_PREFIX}::Core)

  # The app/document fixtures library is defined late (tests/fixtures, after
  # src/). Per-plugin unit tests built during src/ are app-free and don't need
  # it, so only link it when it already exists.
  if(TARGET score_test_fixtures)
    target_link_libraries(${NAME} PRIVATE score_test_fixtures)
  endif()

  if(ARG_GUI OR ARG_APP)
    target_link_libraries(${NAME} PRIVATE
      ${QT_PREFIX}::Gui
      ${QT_PREFIX}::Widgets
      ${QT_PREFIX}::Network
      ${QT_PREFIX}::Xml)
  endif()

  setup_score_common_exe_features(${NAME})
  set_target_properties(${NAME} PROPERTIES FOLDER "Tests")

  add_test(NAME ${NAME} COMMAND ${NAME})

  # App/integration tests rely on runtime dynamic-plugin discovery from
  # "<cwd>/plugins": run them from the build root where <build>/plugins lives.
  if(ARG_APP OR ARG_GUI)
    set_tests_properties(${NAME} PROPERTIES
      WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}"
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SCORE_AUDIO_BACKEND=dummy")
  endif()
endfunction()
