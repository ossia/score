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
#     LIBS      some_other_lib)          # arbitrary extra link libraries
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
  cmake_parse_arguments(ARG "GUI;APP;STANDALONE;SANDBOXED" "" "SOURCES;PLUGINS;LIBS" ${ARGN})

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

  # The fixtures target is defined late (tests/fixtures, after src/), so tests
  # declared from inside src/ -- plug-ins and add-ons -- cannot link it yet. It
  # is header-only, so hand those the include path instead, which is what the
  # else branch below is for.
  if(TARGET score_test_fixtures AND NOT ARG_STANDALONE)
    target_link_libraries(${NAME} PRIVATE score_test_fixtures)
  else()
    target_include_directories(${NAME} PRIVATE "${SCORE_ROOT_SOURCE_DIR}/tests/fixtures")
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

  # App/integration tests rely on runtime dynamic-plugin discovery from
  # "<cwd>/plugins": run them from the build root where <build>/plugins lives.
  if(ARG_APP OR ARG_GUI)
    set_tests_properties(${NAME} PROPERTIES
      WORKING_DIRECTORY "${SCORE_ROOT_BINARY_DIR}")
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
