# ossia score — the suite-registration guard.
#
# A rework of tests/integration/CMakeLists.txt once truncated the file and took
# eleven ctest registrations with it that had nothing to do with the rework.
# Every harness and every source stayed in the tree, so the suite kept LOOKING
# complete while eleven tests no longer ran, and it took three separate audits
# to find them all. Nothing in the build could notice, because nothing compared
# what tests/ registers against what tests/ contains.
#
# This does that comparison and stops the configure when they disagree.
#
# Configure time rather than a ctest entry, on purpose:
#
#   * it runs for everyone who configures with -DSCORE_TESTING=ON, before
#     anything is built, and costs one directory walk;
#   * the one CI job that runs ctest deliberately swallows test failures
#     (ci/coverage.build.sh downgrades a non-zero ctest to a ::warning:: and
#     exits 0) while it aborts on a failed configure, so a FATAL_ERROR here is
#     strictly louder than any test could be;
#   * a test that checks whether tests are registered is itself a registration,
#     and a rewrite that drops registrations can drop that one too — which is
#     the exact failure being guarded.
#
# It is called from the top-level CMakeLists.txt rather than from
# tests/CMakeLists.txt for the same reason: a rework of the test tree must not
# be able to take the check with it.
#
# Silencing a finding means adding the file to one of the lists below, which is
# a visible, reviewable edit that has to say why the file is not a test.

include_guard(GLOBAL)

# Directories under tests/ that the score buildsystem never adds.
set(SCORE_TEST_GUARD_SKIP_DIRS
  # A standalone CMake project: tests/jit-min/CMakeLists.txt declares its own
  # project() and is configured by hand against a system LLVM. cases/*.cpp are
  # inputs the JIT compiles at runtime, not buildsystem sources.
  jit-min
  # Fixture data, no code.
  testdata
)

# Sources under tests/ that no target compiles, by design.
set(SCORE_TEST_GUARD_ALLOWED_SOURCES
)

# Harnesses under tests/ that no ctest entry runs, by design.
set(SCORE_TEST_GUARD_ALLOWED_HARNESSES
  # Registered from the score-addon-aja repository, which is a separate
  # checkout and is not always present. See tests/hardware/README-aja-wiring.md
  # for the score_add_hardware_test() calls that name them.
  hardware/probe-aja.sh
  hardware/probe-decklink.sh
  hardware/probe-magewell.sh
  # Developer tools rather than tests: they were never registered, drive a
  # built score by hand and report to the terminal. Registering them is a
  # separate piece of work, not a regression.
  integration/scene-js-sweep.sh
  integration/video-decoder-sweep.sh
  # The FATE-corpus decode harness (video/corpus-decode-validation). fetch- and
  # generate- pull down or synthesize a multi-gigabyte corpus, and run-corpus /
  # run-hwdec drive it against a built score by hand; none of them can be a ctest
  # entry on a machine that has not fetched the corpus first.
  corpus/fetch-fate-suite.sh
  corpus/generate-corpus.sh
  corpus/run-corpus.sh
  corpus/run-hwdec.sh
)

# Executables built under tests/ that are deliberately not ctest entries.
set(SCORE_TEST_GUARD_ALLOWED_TARGETS
  # A helper binary the shell harnesses drive (${OBJECT_GALLERY} in
  # render-sweep.sh and csf-sweep.sh), not a test of its own.
  ObjectGallery
  # Driven by the corpus/*.sh harnesses above, not by ctest.
  score_video_corpus_tester
  # Helper the plugin-scanner test spawns (static-Qt builds only), started
  # through SCORE_FAKE_PUPPET, never by ctest.
  score_test_fake_puppet
  # Hidden playback-throughput benchmark (every Catch2 case is a [.tag]):
  # it needs media files handed in through VIDEO_BENCH_FILES and prints
  # numbers instead of asserting them, so a bare ctest entry can only ever
  # fail with Catch2's 'No tests ran'. Run by hand per its file header.
  test_gfx_video_direct_bench
)

# Every directory the buildsystem created below `dir`, `dir` included.
function(_score_test_guard_subdirs dir out)
  set(_acc "${dir}")
  get_property(_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_sub IN LISTS _subs)
    _score_test_guard_subdirs("${_sub}" _rec)
    list(APPEND _acc ${_rec})
  endforeach()
  set(${out} "${_acc}" PARENT_SCOPE)
endfunction()

function(score_check_test_registration)
  set(_root "${SCORE_ROOT_SOURCE_DIR}/tests")
  if(NOT IS_DIRECTORY "${_root}")
    return()
  endif()

  # What the buildsystem consumes, read off the targets and tests themselves.
  # Reading the CMakeLists as text instead would let a name surviving in a
  # comment pass for coverage.
  _score_test_guard_subdirs("${_root}" _dirs)

  set(_compiled "")
  set(_registered "")
  set(_executables "")
  foreach(_dir IN LISTS _dirs)
    get_property(_tests DIRECTORY "${_dir}" PROPERTY TESTS)
    list(APPEND _registered ${_tests})

    get_property(_targets DIRECTORY "${_dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
      get_target_property(_type ${_target} TYPE)
      get_target_property(_sources ${_target} SOURCES)
      get_target_property(_srcdir ${_target} SOURCE_DIR)
      if(_type STREQUAL "EXECUTABLE")
        list(APPEND _executables ${_target})
      endif()
      foreach(_source IN LISTS _sources)
        if(NOT IS_ABSOLUTE "${_source}")
          set(_source "${_srcdir}/${_source}")
        endif()
        get_filename_component(_source "${_source}" ABSOLUTE)
        list(APPEND _compiled "${_source}")
      endforeach()
    endforeach()
  endforeach()

  # The second signal: the text of every file that could wire something up,
  # with comments stripped so that a name surviving only in prose counts as
  # gone. It covers what target introspection cannot -- a harness, which is a
  # command and not a source, and anything behind a condition this particular
  # configuration did not take.
  set(_registrations "")
  file(GLOB_RECURSE _cmake_files
    "${_root}/*/CMakeLists.txt"
    "${_root}/CMakeLists.txt"
    "${SCORE_ROOT_SOURCE_DIR}/cmake/Score*.cmake")
  foreach(_file IN LISTS _cmake_files)
    file(READ "${_file}" _text)
    string(REGEX REPLACE "#[^\n]*" "" _text "${_text}")
    string(APPEND _registrations "${_text}")
  endforeach()

  set(_orphans "")

  # CONFIGURE_DEPENDS so that adding a file re-runs the configure by itself.
  # Deleting a registration always edits a CMakeLists and reconfigures anyway;
  # adding a source and never wiring it up touches nothing the buildsystem
  # watches, and that is how the two unregistered harnesses below got in.
  file(GLOB_RECURSE _sources CONFIGURE_DEPENDS RELATIVE "${_root}" "${_root}/*.cpp")
  file(GLOB_RECURSE _harnesses CONFIGURE_DEPENDS RELATIVE "${_root}" "${_root}/*.sh")
  # .py too: assert-content.py sat unwired and unnoticed for a whole campaign
  # because this glob only looked at .cpp and .sh.
  file(GLOB_RECURSE _pysources CONFIGURE_DEPENDS RELATIVE "${_root}" "${_root}/*.py")
  list(APPEND _harnesses ${_pysources})

  foreach(_source IN LISTS _sources)
    string(REGEX MATCH "^[^/]+" _top "${_source}")
    if(_top IN_LIST SCORE_TEST_GUARD_SKIP_DIRS)
      continue()
    endif()
    if(_source IN_LIST SCORE_TEST_GUARD_ALLOWED_SOURCES)
      continue()
    endif()
    get_filename_component(_name "${_source}" NAME)
    string(FIND "${_registrations}" "${_name}" _found)
    if(NOT "${_root}/${_source}" IN_LIST _compiled AND _found EQUAL -1)
      list(APPEND _orphans "  tests/${_source}: no target compiles it, no CMakeLists names it")
    endif()
  endforeach()

  foreach(_harness IN LISTS _harnesses)
    string(REGEX MATCH "^[^/]+" _top "${_harness}")
    if(_top IN_LIST SCORE_TEST_GUARD_SKIP_DIRS)
      continue()
    endif()
    if(_harness IN_LIST SCORE_TEST_GUARD_ALLOWED_HARNESSES)
      continue()
    endif()
    get_filename_component(_name "${_harness}" NAME)
    string(FIND "${_registrations}" "${_name}" _found)
    if(_found EQUAL -1)
      list(APPEND _orphans "  tests/${_harness}: no CMakeLists under tests/ names it")
    endif()
  endforeach()

  foreach(_target IN LISTS _executables)
    if(_target IN_LIST SCORE_TEST_GUARD_ALLOWED_TARGETS)
      continue()
    endif()
    set(_has_test 0)
    foreach(_test IN LISTS _registered)
      # Not equality: score_add_hardware_test() and score_add_media_test()
      # register one harness executable under several test names.
      string(FIND "${_test}" "${_target}" _found)
      if(NOT _found EQUAL -1)
        set(_has_test 1)
        break()
      endif()
    endforeach()
    # A NO_CTEST target is registered by one of those wrappers instead, which
    # name it after their EXECUTABLE keyword.
    if(NOT _has_test AND _registrations MATCHES "EXECUTABLE[ \t\r\n]+${_target}[ \t\r\n]")
      set(_has_test 1)
    endif()
    if(NOT _has_test)
      list(APPEND _orphans "  target ${_target}: built under tests/, registered as no ctest entry")
    endif()
  endforeach()

  list(LENGTH _registered _n_registered)
  if(_orphans)
    list(JOIN _orphans "\n" _report)
    message(FATAL_ERROR
      "The test tree provides more than the suite runs.\n"
      "${_report}\n\n"
      "Each of these exists in tests/ and no ctest entry reaches it, which is "
      "how eleven integration tests once stopped running while still looking "
      "present. Either register it, or add it to the matching "
      "SCORE_TEST_GUARD_ALLOWED_* list in cmake/ScoreTestRegistrationGuard.cmake "
      "with the reason it is not a test.")
  endif()

  message(STATUS
    "score: tests/ registers ${_n_registered} tests; every source and harness "
    "it contains is accounted for")
endfunction()
