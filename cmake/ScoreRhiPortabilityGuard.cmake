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
#   3. `QRhi::D3D12` and `QRhi::statistics()` both arrived in 6.6. The D3D12 one
#      is worse than a missing name: with the enumerator absent the compiler
#      recovers by reading it as D3D11 and the NEXT error is a duplicate case
#      label, so guarding the enumerator without its case arm does not compile
#      either.
#
# The first two are enforced by shape below (they are spelling rules, and hold
# with no Qt 6.4 anywhere in sight). The third is not a shape at all -- it is
# just "this name did not exist yet" -- and a deny-list of such names would
# only ever be as long as the list of breakages that already shipped. So when
# a Qt 6.4 private RHI header can be found on this machine, the guard also
# runs an ALLOW-LIST: every QRhi API a test names must be declared in that
# header. Distributions ship it as qt6-base-private-dev, which is exactly
# where CI's 6.4.2 comes from, so the check is normally live on CI.
#
# WHAT THE ALLOW-LIST IS NOT: "if src/ uses it, it is safe". That was the
# heuristic used when the guard was first written, and it is what let
# QRhi::statistics() through -- src/plugins/score-plugin-gfx/Gfx/Graph/
# RenderList.cpp:1014 does call statistics(), inside
# `#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)` at :996. The oracle is only
# valid for UNGUARDED uses in src/, and nothing was checking that.
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

# Where to find a Qt 6.4 private RHI header to build the allow-list from.
# Override with -DSCORE_RHI_GUARD_QT64_DIR=/path/to/QtGui/private if the
# auto-detection does not find the distribution's copy.
set(SCORE_RHI_GUARD_QT64_DIR "" CACHE PATH
  "Directory holding Qt 6.4's qrhi*_p.h, used as the allow-list of QRhi API CI can compile")

# Reads every qrhi*_p.h under the located directory into one string, or leaves
# the output empty when no Qt 6.4 is installed here.
function(_score_rhi_read_qt64_headers _out_text _out_dir)
  set(_dir "${SCORE_RHI_GUARD_QT64_DIR}")
  if(NOT _dir)
    file(GLOB _cands
      "/usr/include/*/qt6/QtGui/6.4.*/QtGui/private/qrhi_p.h"
      "/usr/include/qt6/QtGui/6.4.*/QtGui/private/qrhi_p.h"
      "/usr/local/include/*/qt6/QtGui/6.4.*/QtGui/private/qrhi_p.h")
    if(_cands)
      list(GET _cands 0 _first)
      get_filename_component(_dir "${_first}" DIRECTORY)
    endif()
  endif()

  set(_text "")
  if(_dir AND IS_DIRECTORY "${_dir}")
    file(GLOB _hdrs "${_dir}/qrhi*_p.h")
    foreach(_h IN LISTS _hdrs)
      file(READ "${_h}" _t)
      string(APPEND _text "${_t}")
    endforeach()
  endif()
  set(${_out_text} "${_text}" PARENT_SCOPE)
  set(${_out_dir} "${_dir}" PARENT_SCOPE)
endfunction()

# QRhi*-named types the PROJECT declares itself -- score has its own
# QRhiBackendKind (Gfx/Graph/interop/GpuCapabilities.hpp:75) and its own
# QRhiReadbackResult alias -- which are obviously not in any Qt header.
function(_score_rhi_project_types _out)
  set(_types "")
  file(GLOB_RECURSE _srcs
    "${SCORE_ROOT_SOURCE_DIR}/src/*.hpp"
    "${SCORE_ROOT_SOURCE_DIR}/src/*.h"
    "${SCORE_ROOT_SOURCE_DIR}/src/*.cpp")
  foreach(_f IN LISTS _srcs)
    file(READ "${_f}" _t)
    if(NOT _t MATCHES "QRhi")
      continue()
    endif()
    string(REGEX MATCHALL
      "(class|struct|enum|enum[ \t]+class|using)[ \t]+QRhi[A-Za-z0-9_]*"
      _m "${_t}")
    foreach(_d IN LISTS _m)
      string(REGEX REPLACE "^.*[ \t]+" "" _d "${_d}")
      list(APPEND _types "${_d}")
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _types)
  set(${_out} "${_types}" PARENT_SCOPE)
endfunction()

# True when `_id` appears as a whole identifier anywhere in `_text`.
function(_score_rhi_declared _text _id _out)
  set(_r FALSE)
  if(_text MATCHES "(^|[^A-Za-z0-9_])${_id}([^A-Za-z0-9_]|$)")
    set(_r TRUE)
  endif()
  set(${_out} ${_r} PARENT_SCOPE)
endfunction()

function(score_check_rhi_portability)
  set(_root "${SCORE_ROOT_SOURCE_DIR}/tests")
  if(NOT IS_DIRECTORY "${_root}")
    return()
  endif()

  _score_rhi_read_qt64_headers(_qt64 _qt64dir)
  set(_projtypes "")
  if(_qt64)
    _score_rhi_project_types(_projtypes)
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
    # <= 6.5"; those must not trip the checks, so match on code only. Ordinary
    # string literals go too -- GfxRenderPassLeak.cpp prints the word
    # "QRhiStats" in an INFO message.
    string(REGEX REPLACE "//[^\n]*" "" _code "${_txt}")
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_code}")
    string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "\"\"" _code "${_code}")

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

    # --- 3. allow-list: every QRhi API named must exist in Qt 6.4 -------------
    # Only when a 6.4 header was found. A file carrying its own Qt-version
    # guard is exempt, because the check is textual and cannot tell which side
    # of an #if a name is on -- GfxIndirectDrawCount.cpp legitimately names
    # QRhi::DrawIndirect inside QT_VERSION_CHECK(6, 12, 0). That is the known
    # hole; it is narrow, and it is the same exemption rule 1 already uses.
    if(_qt64
       AND NOT _txt MATCHES "QT_VERSION_CHECK"
       AND NOT _txt MATCHES "__has_include\\(<rhi/qrhi_platform.h>\\)")
      set(_ids "")

      # QRhi::Enumerator / QRhi::staticMember
      string(REGEX MATCHALL "QRhi::[A-Za-z_][A-Za-z0-9_]*" _m "${_code}")
      foreach(_x IN LISTS _m)
        string(REPLACE "QRhi::" "" _x "${_x}")
        list(APPEND _ids "${_x}")
      endforeach()

      # A call through a receiver named `rhi` (or `<something>rhi`), which in
      # this tree is always a QRhi*: `rs->rhi->statistics()`, `st->rhi->...`.
      string(REGEX MATCHALL
        "rhi->[ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*\\(" _m "${_code}")
      foreach(_x IN LISTS _m)
        string(REGEX REPLACE "^rhi->[ \t\r\n]*" "" _x "${_x}")
        string(REGEX REPLACE "[ \t\r\n]*\\($" "" _x "${_x}")
        list(APPEND _ids "${_x}")
      endforeach()

      # QRhi-prefixed type names.
      string(REGEX MATCHALL "QRhi[A-Za-z0-9_]*" _m "${_code}")
      list(APPEND _ids ${_m})

      if(_ids)
        list(REMOVE_DUPLICATES _ids)
      endif()
      foreach(_id IN LISTS _ids)
        if(_id IN_LIST _projtypes)
          continue()
        endif()
        _score_rhi_declared("${_qt64}" "${_id}" _known)
        if(NOT _known)
          string(CONCAT _msg
            "  tests/${_rel}\n"
            "      names QRhi API `${_id}`, which does not exist in Qt 6.4.\n"
            "      (checked against ${_qt64dir})\n"
            "      Guard it with #if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0),\n"
            "      and guard the whole construct it sits in -- a missing\n"
            "      enumerator makes the compiler recover as a NEIGHBOURING one,\n"
            "      so a switch arm guarded only at its label becomes a duplicate\n"
            "      case. If the API is a counting/telemetry extra, compile the\n"
            "      mechanism out below 6.6 rather than weakening what is\n"
            "      asserted on 6.6+.")
          list(APPEND _bad "${_msg}")
        endif()
      endforeach()
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

  if(_qt64)
    message(STATUS
      "score: ${_checked} RHI-touching test sources check out against Qt 6.4"
      " (allow-list from ${_qt64dir})")
  else()
    message(STATUS
      "score: ${_checked} RHI-touching test sources check out against Qt 6.4"
      " (shape rules only -- no Qt 6.4 headers here; install qt6-base-private-dev"
      " or set SCORE_RHI_GUARD_QT64_DIR to enable the allow-list)")
  endif()
endfunction()
