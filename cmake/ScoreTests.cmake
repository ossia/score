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
#     STANDALONE                         # do not link score_lib_base, only use its headers
#     NO_CTEST                           # build the executable, register it elsewhere
#     LIBS      some_other_lib)          # arbitrary extra link libraries
#
# NO_CTEST is for a harness whose ctest entry needs a wrapper this function does
# not know about -- score_add_media_test(), which provisions the media stack the
# harness reads. Without it the harness would also be registered as a bare
# ctest entry that runs with none of it provisioned.
#
# STANDALONE is for tests that recompile a score_lib_base source into the test
# and substitute one of its dependencies: linking the library as well would
# define the same symbols twice, which only some linkers resolve in favour of
# the executable.
#
# A plain test links score_lib_base + score_test_fixtures + Catch2 and runs as a
# single ctest entry. An APP/GUI test additionally runs from the build root so
# that the dynamic plugins in <build>/plugins are discovered at runtime
# (see score::PluginLoader::pluginsDir(), which probes "<cwd>/plugins").

include_guard(GLOBAL)

# The .sh harnesses need flock/oscsend and POSIX tooling. On Windows ctest
# cannot execute them at all and reports BAD_COMMAND, which counts as a failure
# rather than a skip. Global rather than per-directory: tests/integration and
# tests/hardware both gate on it, and a directory-scoped copy is invisible to
# the score_add_media_test() function called from the latter.
if(WIN32)
  set(SCORE_HAS_SHELL_HARNESS 0 CACHE INTERNAL "shell harnesses are runnable")
else()
  set(SCORE_HAS_SHELL_HARNESS 1 CACHE INTERNAL "shell harnesses are runnable")
endif()

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

# Sources a test compiles directly because the shared plugin hides them
# (visibility). A static plugin archive already provides them; compiling them
# again duplicates every symbol at link time.
function(score_plugin_hidden_sources OUT)
  if(SCORE_STATIC_PLUGINS OR NOT BUILD_SHARED_LIBS)
    set(${OUT} "" PARENT_SCOPE)
  else()
    set(${OUT} ${ARGN} PARENT_SCOPE)
  endif()
endfunction()

function(score_add_test NAME)
  cmake_parse_arguments(ARG "GUI;APP;STANDALONE;SANDBOXED;NO_CTEST" "" "SOURCES;PLUGINS;LIBS" ${ARGN})

  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "score_add_test(${NAME}): no SOURCES given")
  endif()

  add_executable(${NAME} ${ARG_SOURCES})

  # score_lib_pch is compiled for the shared libraries (-fPIC) while an
  # executable gets -fPIE: clang refuses to reuse a PCH across that difference,
  # so opt the tests out of it (SCORE_PCH builds only).
  set_target_properties(${NAME} PROPERTIES SCORE_CUSTOM_PCH 1)

  if(ARG_STANDALONE)
    target_include_directories(${NAME} PRIVATE
      $<TARGET_PROPERTY:score_lib_base,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(${NAME} PRIVATE
      $<TARGET_PROPERTY:score_lib_base,INTERFACE_COMPILE_DEFINITIONS>)
  else()
    target_link_libraries(${NAME} PRIVATE score_lib_base)
  endif()

  target_link_libraries(${NAME} PRIVATE
    Catch2::Catch2WithMain
    ${ARG_PLUGINS}
    ${ARG_LIBS}
    ${QT_PREFIX}::Core)

  # The app/document fixtures library is defined late (tests/fixtures, after
  # src/). Per-plugin unit tests built during src/ are app-free and don't need
  # it, so only link it when it already exists.
  if(TARGET score_test_fixtures AND NOT ARG_STANDALONE)
    target_link_libraries(${NAME} PRIVATE score_test_fixtures)
  endif()

  if(ARG_GUI OR ARG_APP)
    target_link_libraries(${NAME} PRIVATE
      ${QT_PREFIX}::Gui
      ${QT_PREFIX}::Widgets
      ${QT_PREFIX}::Network
      ${QT_PREFIX}::Xml)

    if(SCORE_STATIC_PLUGINS)
      set(_test_plugins "${SCORE_PLUGINS_LIST}")
      list(REMOVE_ITEM _test_plugins score_plugin_jit)
      target_link_libraries(${NAME} PRIVATE ${_test_plugins})
    endif()
  endif()

  setup_score_common_exe_features(${NAME})
  score_wasm_batch_program(${NAME})

  if(ARG_GUI)
    enable_minimal_qt_plugins(${NAME} 1)
  else()
    enable_minimal_qt_plugins(${NAME} 0)
  endif()

  set_target_properties(${NAME} PROPERTIES FOLDER "Tests")

  if(ARG_NO_CTEST)
    return()
  endif()

  # A test that exercises code which deletes or overwrites media files runs
  # with the filesystem read-only apart from /tmp, so a bug in it cannot reach
  # anything of the developer's. See tests/tools/sandboxed-test.sh.
  if(ARG_SANDBOXED AND UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    add_test(NAME ${NAME}
      COMMAND "${SCORE_ROOT_SOURCE_DIR}/tests/tools/sandboxed-test.sh"
              "$<TARGET_FILE:${NAME}>")
  else()
    add_test(NAME ${NAME} COMMAND ${NAME})
  endif()

  # Catch2 exits with 4 when every test case in the binary was skipped
  # (AllTestsSkippedExitCode, catch_session.cpp). A test that skips because its
  # precondition is absent -- no display, no shader library, no capture device --
  # is not a defect, and counting it as one silently inflates the failure count.
  set_tests_properties(${NAME} PROPERTIES SKIP_RETURN_CODE 4)

  # App/integration tests rely on runtime dynamic-plugin discovery from
  # "<cwd>/plugins": run them from the build root where <build>/plugins lives.
  if(ARG_APP OR ARG_GUI)
    set_tests_properties(${NAME} PROPERTIES
      WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")

    # ...and tell the fixture where that is, so running the executable by hand
    # from some other directory boots the same application instead of one with
    # no plug-ins at all. See prepare_test_environment().
    target_compile_definitions(${NAME} PRIVATE
      "SCORE_TEST_BINARY_DIR=\"${SCORE_ROOT_BINARY_DIR}\"")
  endif()

  if(ARG_APP)
    # Headless: force the offscreen platform.
    set_tests_properties(${NAME} PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SCORE_AUDIO_BACKEND=dummy;SCORE_DISABLE_AUDIOPLUGINS=1")
  elseif(ARG_GUI)
    # GUI tests need a real display (X11 locally, Xvfb in CI): do NOT force
    # offscreen. Labelled "gui" so CI can gate them behind a display.
    set_tests_properties(${NAME} PROPERTIES
      ENVIRONMENT "SCORE_AUDIO_BACKEND=dummy;SCORE_DISABLE_AUDIOPLUGINS=1"
      LABELS "gui")
  endif()
endfunction()
