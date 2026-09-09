# ossia score — the RHI portability guard.
#
# CI BUILDS AGAINST Qt 6.4.2. The local reference build is Qt 6.13. Repeatedly
# now, code has compiled here and broken the Coverage job there, and it is
# always the same shape: an API that only exists, or only has that signature,
# or is only callable that way, in a Qt newer than CI's.
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
#   4. `mipLevelsForSize` is a non-static MEMBER on 6.4
#      (`int mipLevelsForSize(const QSize &) const`) and became static later.
#      `QRhi::mipLevelsForSize(sz)` therefore compiles on 6.13 and fails on 6.4
#      with "call to non-static member function without an object argument".
#      The NAME exists in both, so an existence check cannot see this: the
#      thing that changed is the CALL FORM.
#
# Rules 1, 2 and 4 are enforced by shape below; they are spelling rules and
# hold with no Qt 6.4 anywhere in sight, except that 4 needs the 6.4 header to
# know which members are static there. Rule 3 is not a shape at all -- it is
# just "this name did not exist yet" -- and a deny-list of such names would
# only ever be as long as the list of breakages that already shipped. So when
# a Qt 6.4 private RHI header can be found on this machine, the guard also
# runs an ALLOW-LIST: every QRhi API named must be declared in that header.
# Distributions ship it as qt6-base-private-dev, which is exactly where CI's
# 6.4.2 comes from, so the check is normally live on CI.
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
# SCOPE. The rules do not all cover the same tree, and the reason is not
# uniform:
#
#   * Rules 2 and 4 (pure spelling/call-form) run over BOTH tests/ AND src/.
#     "Product code is compiled by every CI job, so a mistake there is caught
#     loudly by all of them" does not hold for VERSION mistakes: every job
#     builds against its own Qt, and only the Coverage job builds against
#     6.4.2. A 6.4-only break in src/ is therefore caught by exactly one job,
#     no more loudly than one in tests/, so both trees need the same rules.
#
#   * Rule 1 stays tests/-only. Four files under src/ use a concrete
#     QRhi*InitParams --
#       src/lib/score/gfx/Vulkan.cpp                        (guarded, __has_include)
#       src/plugins/score-plugin-gfx/Gfx/Graph/Window.cpp
#       src/plugins/score-plugin-gfx/Gfx/Graph/KmsOutputNode.cpp
#       src/addons/score-addon-ndi/Ndi/OutputNode.cpp
#     -- the last three unguarded, reaching the per-backend private header
#     directly. That is correct on CI's 6.4 and on the Qt source tree built
#     here; it would only bite on an INSTALLED Qt >= 6.6, which is outside the
#     current matrix. Left alone rather than made to fail a configure that
#     works today.
#
#   * Rule 3 (the allow-list) stays tests/-only. It is a whole-name check with
#     no notion of #if, so over src/ it would fire on every correctly-guarded
#     newer-Qt use -- RenderList.cpp's statistics() call being the obvious one.
#
# All of it runs only under SCORE_TESTING (CMakeLists.txt:348), which is where
# this file is included from. That is the right trigger -- the Coverage job on
# 6.4.2 builds with testing on, so the src/ rules reach CI -- but it does mean
# a plain non-testing configure checks nothing, and a src/ break will not be
# reported by one.

# Paths exempted from a check, with the reason. Relative to the source root.
# Format:
#   tests/relative/path.cpp
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

# Replaces every C++ raw string literal with an empty "" .
#
# Must run BEFORE the quote-based string stripper, which cannot survive them:
# a raw string may legally contain bare `"` characters, and this tree is full
# of them -- GLSL, regexes, HLSL. 105 of the 397 first-party QRhi-touching
# files hold at least one. One `R"RX(... device="(.*)" ...)RX"` in
# tests/integration/ThreedimRenderTest.cpp mis-pairs every following quote in
# the file, which left `QRhi::backendName()` -- a name that appears there only
# inside comments and a SKIP() message -- looking like live code to rule 4.
#
# Written as a FIND/SUBSTRING loop, not a regex: the delimiter has to match at
# both ends and CMake regex has no backreferences, and a nested-star
# approximation of one risks the catastrophic backtracking the block-comment
# pattern below is already prone to. This is linear.
function(_score_rhi_strip_raw _in _out)
  set(_rest "${_in}")
  set(_acc "")
  while(TRUE)
    string(FIND "${_rest}" "R\"" _p)
    if(_p LESS 0)
      string(APPEND _acc "${_rest}")
      break()
    endif()
    string(SUBSTRING "${_rest}" 0 ${_p} _head)
    math(EXPR _after "${_p} + 2")
    string(SUBSTRING "${_rest}" ${_after} -1 _tail)

    string(FIND "${_tail}" "(" _op)
    set(_ok TRUE)
    if(_op LESS 0)
      set(_ok FALSE)
    else()
      string(SUBSTRING "${_tail}" 0 ${_op} _delim)
      # A real raw-string delimiter is short and has no whitespace or parens.
      # Anything else means this `R"` was not a raw string at all (an `R` that
      # merely precedes an ordinary string, say), so it is left alone.
      string(LENGTH "${_delim}" _dl)
      if(_dl GREATER 16 OR _delim MATCHES "[ \t\r\n()\\\\\"]")
        set(_ok FALSE)
      endif()
    endif()

    if(NOT _ok)
      string(APPEND _acc "${_head}R\"")
      set(_rest "${_tail}")
      continue()
    endif()

    math(EXPR _bstart "${_op} + 1")
    string(SUBSTRING "${_tail}" ${_bstart} -1 _body)
    set(_close ")${_delim}\"")
    string(FIND "${_body}" "${_close}" _cp)
    if(_cp LESS 0)
      # Unterminated: keep the head and stop rather than guess.
      string(APPEND _acc "${_head}")
      break()
    endif()
    string(APPEND _acc "${_head}\"\"")
    string(LENGTH "${_close}" _cl)
    math(EXPR _next "${_cp} + ${_cl}")
    string(SUBSTRING "${_body}" ${_next} -1 _rest)
  endwhile()
  set(${_out} "${_acc}" PARENT_SCOPE)
endfunction()

# True when `_id` appears as a whole identifier anywhere in `_text`.
function(_score_rhi_declared _text _id _out)
  set(_r FALSE)
  if(_text MATCHES "(^|[^A-Za-z0-9_])${_id}([^A-Za-z0-9_]|$)")
    set(_r TRUE)
  endif()
  set(${_out} ${_r} PARENT_SCOPE)
endfunction()

# Removes every `#if QT_VERSION ... #endif` region, nesting included, so what
# is left is the code compiled on EVERY Qt.
#
# Rule 4 needs this because its question -- "is this call form valid on 6.4?"
# -- is meaningless inside a block that only compiles on newer Qt, where the
# member IS static. The exemption has to be per-REGION, not per-file:
# RenderedRawRasterPipelineNode.cpp carries four unrelated version guards
# (:1227, :1311, :1629, :2703), so exempting every file that merely mentions
# QT_VERSION_CHECK would blind the rule to all the unguarded code in it.
#
# `#if` is matched as a substring, so `#ifdef` and `#ifndef` open a region too
# and are balanced correctly against their `#endif`.
function(_score_rhi_strip_version_guarded _in _out)
  set(_rest "${_in}")
  set(_acc "")
  set(_iter 0)
  while(TRUE)
    math(EXPR _iter "${_iter} + 1")
    if(_iter GREATER 2000)
      break()
    endif()
    string(FIND "${_rest}" "#if QT_VERSION" _p)
    if(_p LESS 0)
      string(APPEND _acc "${_rest}")
      break()
    endif()
    string(SUBSTRING "${_rest}" 0 ${_p} _head)
    string(APPEND _acc "${_head}")

    # Walk forward from this directive, balancing #if against #endif.
    math(EXPR _cur "${_p} + 3")
    set(_depth 1)
    set(_done FALSE)
    while(NOT _done)
      math(EXPR _iter "${_iter} + 1")
      if(_iter GREATER 2000)
        set(_done TRUE)
        break()
      endif()
      string(SUBSTRING "${_rest}" ${_cur} -1 _win)
      string(FIND "${_win}" "#if" _ni)
      string(FIND "${_win}" "#endif" _ne)
      if(_ne LESS 0)
        # Unbalanced; drop the remainder rather than guess.
        set(_rest "")
        set(_done TRUE)
        break()
      endif()
      if(_ni GREATER_EQUAL 0 AND _ni LESS _ne)
        math(EXPR _depth "${_depth} + 1")
        math(EXPR _cur "${_cur} + ${_ni} + 3")
      else()
        math(EXPR _depth "${_depth} - 1")
        math(EXPR _cur "${_cur} + ${_ne} + 6")
        if(_depth EQUAL 0)
          string(SUBSTRING "${_rest}" ${_cur} -1 _rest)
          set(_done TRUE)
        endif()
      endif()
    endwhile()
    if(_rest STREQUAL "")
      break()
    endif()
  endwhile()
  set(${_out} "${_acc}" PARENT_SCOPE)
endfunction()

# Builds, ONCE, two lists from the Qt 6.4 headers: every name declared as a
# function, and the subset of those declared `static`.
#
# Done once rather than per-identifier on purpose: a `MATCHALL` with a greedy
# `[^;{}\n]*` prefix, run across the concatenated headers for every identifier
# in every file, segfaults cmake. A segfault here exits 139 with nothing on
# either stream, which reads exactly like a silent pass unless the exit code
# is checked. Both patterns below are anchored on a literal and run once.
function(_score_rhi_func_tables _text _out_all _out_static)
  set(_all "")
  set(_static "")

  string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*[ \t]*\\(" _f "${_text}")
  foreach(_x IN LISTS _f)
    string(REGEX REPLACE "[ \t]*\\($" "" _x "${_x}")
    list(APPEND _all "${_x}")
  endforeach()

  string(REGEX MATCHALL "static[^;{}\n]*\\(" _s "${_text}")
  foreach(_x IN LISTS _s)
    string(REGEX REPLACE "[ \t]*\\($" "" _x "${_x}")
    string(REGEX REPLACE "^.*[^A-Za-z0-9_]" "" _x "${_x}")
    if(_x)
      list(APPEND _static "${_x}")
    endif()
  endforeach()

  if(_all)
    list(REMOVE_DUPLICATES _all)
  endif()
  if(_static)
    list(REMOVE_DUPLICATES _static)
  endif()
  set(${_out_all} "${_all}" PARENT_SCOPE)
  set(${_out_static} "${_static}" PARENT_SCOPE)
endfunction()

function(score_check_rhi_portability)
  set(_testroot "${SCORE_ROOT_SOURCE_DIR}/tests")
  if(NOT IS_DIRECTORY "${_testroot}")
    return()
  endif()

  _score_rhi_read_qt64_headers(_qt64 _qt64dir)
  set(_projtypes "")
  set(_qtfuncs "")
  set(_qtstatics "")
  if(_qt64)
    _score_rhi_project_types(_projtypes)
    _score_rhi_func_tables("${_qt64}" _qtfuncs _qtstatics)
  endif()

  file(GLOB_RECURSE _testfiles
    "${_testroot}/*.cpp" "${_testroot}/*.hpp" "${_testroot}/*.h")
  file(GLOB_RECURSE _srcfiles
    "${SCORE_ROOT_SOURCE_DIR}/src/*.cpp"
    "${SCORE_ROOT_SOURCE_DIR}/src/*.hpp"
    "${SCORE_ROOT_SOURCE_DIR}/src/*.h")

  set(_bad "")
  set(_checked 0)
  foreach(_f IN LISTS _testfiles _srcfiles)
    file(RELATIVE_PATH _rel "${SCORE_ROOT_SOURCE_DIR}" "${_f}")
    if(_rel IN_LIST SCORE_RHI_GUARD_ALLOWED)
      continue()
    endif()
    string(REGEX MATCH "^tests/" _is_test "${_rel}")

    # Vendored code is not ours to hold to score's portability rules, and
    # skipping it also keeps this guard from crashing: the block-comment
    # stripper below is the textbook `/\*([^*]|\*[^/])*\*/`, which backtracks
    # catastrophically on a large comment-dense file. score-plugin-gfx's
    # 3rdparty/libisf/src/isf.cpp (~239 kB) is enough to segfault cmake, and a
    # segfault exits 139 with nothing on either stream, which looks exactly
    # like a clean pass unless the exit code is checked.
    if(_rel MATCHES "(^|/)3rdparty/")
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
    # ORDER MATTERS. Strings are blanked BEFORE comments, because a string may
    # legally contain `//`, and ThreedimRenderTest.cpp:186 does --
    #   o += QStringLiteral("f %1//%2 %3//%4 %5//%6\n")
    # an OBJ face-index format. Stripping comments first eats that string's
    # CLOSING QUOTE, which mis-pairs every quote in the rest of the file; the
    # visible symptom was rule 4 reporting QRhi::backendName(), a name that
    # appears there only in comments and a SKIP() message.
    #
    # The string pattern also excludes \n, so an unbalanced quote (in a
    # comment, say) can spoil at most its own line instead of cascading. C++
    # string literals cannot span a newline anyway -- adjacent literals are
    # separate literals, one per line, each matched on its own.
    _score_rhi_strip_raw("${_txt}" _code)
    string(REGEX REPLACE "\"([^\"\\\\\n]|\\\\.)*\"" "\"\"" _code "${_code}")
    string(REGEX REPLACE "//[^\n]*" "" _code "${_code}")
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_code}")

    # --- 1. concrete QRhi*InitParams (tests/ only, see SCOPE) -----------------
    # `QRhi[A-Za-z0-9]+InitParams` cannot match the base `QRhiInitParams`: the
    # `+` has to consume at least one character before "InitParams".
    if(_is_test AND _code MATCHES "QRhi[A-Za-z0-9]+InitParams")
      # Either guard style is accepted: the version check used by the tests, or
      # the __has_include probe used by src/lib/score/gfx/Vulkan.cpp.
      if(NOT _txt MATCHES "QT_VERSION_CHECK\\(6, *6, *0\\)"
         AND NOT _txt MATCHES "__has_include\\(<rhi/qrhi_platform.h>\\)")
        string(CONCAT _msg
          "  ${_rel}\n"
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

    # --- 2. buffer readback result type, PER CALL SITE ------------------------
    # Checked one readback at a time, by looking at how the variable whose
    # address is passed was DECLARED.
    #
    # The question has to be per-site. A file-scoped one -- "does the word
    # QRhiBufferReadbackResult appear anywhere in this file?" -- lets one
    # correct readback exempt every incorrect one beside it, and a file doing
    # several readbacks is the normal case here.
    set(_sites_seen FALSE)
    string(REGEX MATCHALL
      "readBackBuffer[ \t\r\n]*\\([^;)]*&[ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*\\)"
      _rb "${_code}")
    foreach(_call IN LISTS _rb)
      set(_sites_seen TRUE)
      string(REGEX REPLACE "^.*&[ \t\r\n]*" "" _var "${_call}")
      string(REGEX REPLACE "[ \t\r\n]*\\)$" "" _var "${_var}")
      # A variable declared BOTH ways is the correct #if/#else portability
      # shape, not a defect -- GfxReviewGraphLifecycle.cpp:148 does exactly
      # that around `rb`. Only a variable declared solely with the newer
      # spelling is wrong. Asked per-variable rather than per-file: the whole
      # point of this rule is that a file-scoped question let the real break
      # through.
      if(_code MATCHES "(^|[^A-Za-z0-9_])QRhiReadbackResult[ \t\r\n]+${_var}([^A-Za-z0-9_]|$)"
         AND NOT _code MATCHES "(^|[^A-Za-z0-9_])QRhiBufferReadbackResult[ \t\r\n]+${_var}([^A-Za-z0-9_]|$)")
        string(CONCAT _msg
          "  ${_rel}\n"
          "      readBackBuffer(..., &${_var}) where `${_var}` is declared\n"
          "      QRhiReadbackResult. On Qt 6.4 that parameter is\n"
          "      QRhiBufferReadbackResult*, a DIFFERENT type. Declare `${_var}`\n"
          "      QRhiBufferReadbackResult: Qt 6.4 has it natively and\n"
          "      Gfx/Graph/RenderState.hpp aliases it for newer Qt, so the one\n"
          "      spelling compiles everywhere.")
        list(APPEND _bad "${_msg}")
      endif()
    endforeach()

    # Fallback for call shapes the per-site matcher cannot read (a pointer
    # passed straight through, say). File-scoped, and so still maskable -- but
    # it only applies where no `&var` site was found at all.
    if(NOT _sites_seen
       AND _code MATCHES "readBackBuffer[ \t\r\n]*\\("
       AND _code MATCHES "QRhiReadbackResult"
       AND NOT _code MATCHES "QRhiBufferReadbackResult")
      string(CONCAT _msg
        "  ${_rel}\n"
        "      calls readBackBuffer() while naming only QRhiReadbackResult.\n"
        "      On Qt 6.4 the parameter is QRhiBufferReadbackResult*, which is a\n"
        "      DIFFERENT type. Spell it QRhiBufferReadbackResult: Qt 6.4 has it\n"
        "      natively and Gfx/Graph/RenderState.hpp aliases it for newer Qt,\n"
        "      so the one spelling compiles everywhere.")
      list(APPEND _bad "${_msg}")
    endif()

    # --- 4. QRhi::member() called as a static ---------------------------------
    # Needs the 6.4 header, because the question is not whether the name exists
    # but whether it is static THERE. A name absent from 6.4 entirely is left
    # to rule 3, so that a missing API is reported once rather than twice.
    if(_qt64)
      _score_rhi_strip_version_guarded("${_code}" _always)
      string(REGEX MATCHALL "QRhi::[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*\\(" _calls "${_always}")
      if(_calls)
        list(REMOVE_DUPLICATES _calls)
      endif()
      foreach(_c IN LISTS _calls)
        string(REGEX REPLACE "^QRhi::" "" _name "${_c}")
        string(REGEX REPLACE "[ \t\r\n]*\\($" "" _name "${_name}")
        if(_name IN_LIST _projtypes)
          continue()
        endif()
        if(_name IN_LIST _qtfuncs AND NOT _name IN_LIST _qtstatics)
          string(CONCAT _msg
            "  ${_rel}\n"
            "      calls QRhi::${_name}() as a static function. On Qt 6.4 it is a\n"
            "      non-static member (`... ${_name}(...) const`), so this is\n"
            "      \"call to non-static member function without an object\n"
            "      argument\" there, while it compiles on newer Qt where the\n"
            "      member became static. Call it through an instance instead:\n"
            "      `rhi.${_name}(...)` / `rhi->${_name}(...)`.")
          list(APPEND _bad "${_msg}")
        endif()
      endforeach()
    endif()

    # --- 3. allow-list: every QRhi API named must exist in Qt 6.4 -------------
    # tests/ only (see SCOPE). A file carrying its own Qt-version guard is
    # exempt, because the check is textual and cannot tell which side of an #if
    # a name is on -- GfxIndirectDrawCount.cpp legitimately names
    # QRhi::DrawIndirect inside QT_VERSION_CHECK(6, 12, 0). That is the known
    # hole; it is narrow, and it is the same exemption rule 1 already uses.
    if(_is_test AND _qt64
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
            "  ${_rel}\n"
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
      "A source file uses a QRhi API newer than the Qt CI builds against (6.4.2).\n"
      "${_report}\n\n"
      "This compiles locally on Qt 6.13 and fails the Coverage job. If a file "
      "genuinely cannot follow the rule, add its source-root-relative path to "
      "SCORE_RHI_GUARD_ALLOWED in cmake/ScoreRhiPortabilityGuard.cmake with the "
      "reason.")
  endif()

  if(_qt64)
    message(STATUS
      "score: ${_checked} RHI-touching sources check out against Qt 6.4"
      " (allow-list from ${_qt64dir})")
  else()
    message(STATUS
      "score: ${_checked} RHI-touching sources check out against Qt 6.4"
      " (shape rules only -- no Qt 6.4 headers here; install qt6-base-private-dev"
      " or set SCORE_RHI_GUARD_QT64_DIR to enable the allow-list)")
  endif()
endfunction()
