# ossia score — the RHI portability guard for tests/.
#
# CI BUILDS THE TESTS AGAINST Qt 6.4.2. The local reference build is Qt 6.13.
# Twice now a test has compiled here and broken the Coverage job there, and both
# times it was the same shape: an API that only exists, or only has that
# signature, in a Qt newer than CI's.
#
#   1. `QRhiNullInitParams` / `QRhiGles2InitParams` / ... moved. Through 6.5
#      they live in the per-backend private headers
#      (<QtGui/private/qrhinull_p.h> and friends); from 6.6 they live in the
#      public <rhi/qrhi_platform.h>, which does not exist on 6.4. Including
#      qrhi_p.h alone gets you the BASE QRhiInitParams and nothing else, so the
#      failure is a "no type named" error on the concrete one.
#
#   2. A BUFFER readback result is `QRhiBufferReadbackResult` on 6.4 —
#      `readBackBuffer(QRhiBuffer*, int, int, QRhiBufferReadbackResult*)` — and
#      folds into `QRhiReadbackResult` later. Writing the newer spelling gives
#      "cannot initialize a parameter of type 'QRhiBufferReadbackResult *' with
#      an rvalue of type 'QRhiReadbackResult *'". The project already has the
#      compatibility alias in Gfx/Graph/RenderState.hpp, so the portable answer
#      is simply to use the older name everywhere.
#
# A convention nothing enforces comes back, so this enforces the two known
# shapes. It is deliberately narrow: catching the two classes that have actually
# shipped is worth more than a general Qt-version analysis that nobody trusts.
#
# Configure time rather than a ctest entry, for the same reasons as
# ScoreTestRegistrationGuard: these are COMPILE failures, so a test could never
# run to report them, and the one CI job that runs ctest downgrades test
# failures to a warning while it aborts on a failed configure.
#
# SCOPE: tests/ only, on purpose. Product code is compiled by every CI job, so a
# version mistake there is caught immediately and loudly by all of them; test
# code is compiled by the Coverage job alone, which is why this is where it
# bites. Measured while writing this guard: four files under src/ do use a
# concrete QRhi*InitParams —
#   src/lib/score/gfx/Vulkan.cpp                        (guarded, __has_include)
#   src/plugins/score-plugin-gfx/Gfx/Graph/Window.cpp
#   src/plugins/score-plugin-gfx/Gfx/Graph/KmsOutputNode.cpp
#   src/addons/score-addon-ndi/Ndi/OutputNode.cpp
# — the last three unguarded, reaching the per-backend private header directly.
# That is correct on CI's 6.4 and on the Qt source tree built here; it would
# only bite on an INSTALLED Qt >= 6.6, which is outside the current matrix.
# Left alone rather than made to fail a configure that works today.

# Files under tests/ exempted from a check, with the reason. Format:
#   relative/path.cpp
set(SCORE_RHI_GUARD_ALLOWED
  # (empty — add a path here with the reason it cannot follow the rule)
)

function(score_check_rhi_portability)
  set(_root "${SCORE_ROOT_SOURCE_DIR}/tests")
  if(NOT IS_DIRECTORY "${_root}")
    return()
  endif()

  file(GLOB_RECURSE _files
    "${_root}/*.cpp"
    "${_root}/*.hpp"
    "${_root}/*.h")

  set(_bad "")
  set(_checked 0)
  foreach(_f IN LISTS _files)
    file(RELATIVE_PATH _rel "${_root}" "${_f}")
    if(_rel IN_LIST SCORE_RHI_GUARD_ALLOWED)
      continue()
    endif()

    file(READ "${_f}" _txt)
    if(NOT _txt MATCHES "QRhi")
      continue()
    endif()
    math(EXPR _checked "${_checked} + 1")

    # Comments say things like "QRhiBufferReadbackResult: distinct type in Qt
    # <= 6.5"; those must not trip the checks, so match on code only.
    string(REGEX REPLACE "//[^\n]*" "" _code "${_txt}")
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_code}")

    # --- 1. concrete QRhi*InitParams ------------------------------------------
    # `QRhi[A-Za-z0-9]+InitParams` cannot match the base `QRhiInitParams`: the
    # `+` has to consume at least one character before "InitParams".
    if(_code MATCHES "QRhi[A-Za-z0-9]+InitParams")
      # Either guard style is accepted: the version check used by the tests, or
      # the __has_include probe used by src/lib/score/gfx/Vulkan.cpp.
      if(NOT _txt MATCHES "QT_VERSION_CHECK\\(6, *6, *0\\)"
         AND NOT _txt MATCHES "__has_include\\(<rhi/qrhi_platform.h>\\)")
        string(CONCAT _msg
          "  tests/${_rel}\n"
          "      uses a concrete QRhi*InitParams with no Qt-version guard.\n"
          "      Add, next to the qrhi_p.h include:\n"
          "        #if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)\n"
          "        #include <rhi/qrhi_platform.h>\n"
          "        #else\n"
          "        #include <QtGui/private/qrhi{null,gles2,vulkan}_p.h>\n"
          "        #endif")
        list(APPEND _bad "${_msg}")
      endif()
    endif()

    # --- 2. buffer readback result type ---------------------------------------
    # A file that legitimately does both a texture and a buffer readback names
    # QRhiBufferReadbackResult too, so requiring its absence keeps that case out.
    if(_code MATCHES "readBackBuffer[ \t\r\n]*\\("
       AND _code MATCHES "QRhiReadbackResult"
       AND NOT _code MATCHES "QRhiBufferReadbackResult")
      string(CONCAT _msg
        "  tests/${_rel}\n"
        "      calls readBackBuffer() while naming only QRhiReadbackResult.\n"
        "      On Qt 6.4 the parameter is QRhiBufferReadbackResult*, which is a\n"
        "      DIFFERENT type. Spell it QRhiBufferReadbackResult: Qt 6.4 has it\n"
        "      natively and Gfx/Graph/RenderState.hpp aliases it for newer Qt,\n"
        "      so the one spelling compiles everywhere.")
      list(APPEND _bad "${_msg}")
    endif()
  endforeach()

  if(_bad)
    list(JOIN _bad "\n" _report)
    message(FATAL_ERROR
      "A test uses a QRhi API newer than the Qt CI builds against (6.4.2).\n"
      "${_report}\n\n"
      "This compiles locally on Qt 6.13 and fails the Coverage job. If a file "
      "genuinely cannot follow the rule, add its tests/-relative path to "
      "SCORE_RHI_GUARD_ALLOWED in cmake/ScoreRhiPortabilityGuard.cmake with the "
      "reason.")
  endif()

  message(STATUS
    "score: ${_checked} RHI-touching test sources check out against Qt 6.4")
endfunction()
