# ossia score — the expected-red guard.
#
# A test that is RED ON PURPOSE is a valuable thing: it pins a defect we have
# reproduced but not yet fixed, and it turns green the day someone fixes it.
# The problem is not that we have them, it is that we grew FOUR unrelated ways
# of saying so, and no two of them are visible to the same query:
#
#   1. Catch2 `[!shouldfail]` on the TEST_CASE tag string. Catch2 enforces it:
#      the case is expected to fail, and an unexpected PASS is reported as a
#      failure. This is the good one.
#   2. CMake `set_tests_properties(... WILL_FAIL TRUE)`. Needed when the defect
#      ABORTS the process, because Catch2 never gets to report anything — see
#      test_integration_js_rootpath_static.
#   3. A `[finding]` Catch2 tag. Enforces NOTHING. Some of the cases carrying it
#      are genuinely red, some were fixed and kept the tag, and the tag itself
#      cannot tell you which.
#   4. A comment banner in the file, and nothing else at all.
#
# The cost is not theoretical. The Windows d3d11 run produced 11 failures that
# COULD NOT BE TRIAGED MECHANICALLY, because answering "is this one of ours?"
# meant reading four different conventions across three file types by hand.
#
# This guard does not try to abolish (2) — an abort really cannot be encoded in
# Catch2 — nor to forbid (3) and (4), which carry prose a tag never could. It
# does the one thing that makes the set tractable: it requires that EVERY
# expected-red in the tree, whichever mechanism enforces it, is declared once in
# the manifest below, and that every manifest entry still exists. Then a single
# generated file answers the triage question for every platform.
#
# Configure time rather than a ctest entry, for the same reasons as
# ScoreTestRegistrationGuard: the one CI job that runs ctest downgrades test
# failures to a warning while it aborts on a failed configure, and a test that
# checks the expected-red inventory would itself be an entry in that inventory.
#
# WHAT IT WRITES: ${CMAKE_BINARY_DIR}/expected-red.txt, one TSV row per entry.
# That is the file a triage script should read. It is regenerated on every
# configure, so it cannot drift from the tree the way a checked-in list would.
#
# TO ADD A PIN: add a row here. To remove one, delete the row — the guard fails
# if a declared pin no longer exists, which is what makes a fixed defect get its
# bookkeeping cleaned up instead of quietly rotting.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# The manifest. One row per expected-red case.
#
#   mechanism @ path @ case name @ why it is red
#
# mechanism is one of:
#   shouldfail  Catch2 [!shouldfail] tag        — enforced by Catch2
#   will_fail   CMake WILL_FAIL property        — enforced by ctest
#   finding     [finding] tag, red              — ENFORCED BY NOTHING
#   fixed       [finding] tag, now green        — kept for the name only
# ---------------------------------------------------------------------------
set(SCORE_EXPECTED_RED
  # -- Catch2 [!shouldfail] -------------------------------------------------
  "shouldfail@tests/integration/ScenarioContentRoundtripTest.cpp@A scenario with an added process stays a byte fixed point@A10: two non-determinism sources remain — view-geometry doubles recomputed on layout, and a random 62-byte tail"
  "shouldfail@tests/integration/ScenarioContentRoundtripTest.cpp@a scenario with added processes is a JSON byte fixed point@A27: the process-order half is FIXED and its sibling order case is green and enforced. What is left is ONE named source — IntervalModel Zoom/Center, recomputed from the live viewport width by ScenarioDocumentPresenter::on_minimapChanged and written back into the model. A view-behavior change, not a serialization one"
  "shouldfail@tests/integration/MissingProcessRoundtripTest.cpp@a process whose factory is missing keeps its identity, its ports and its cables across a load@A16: ProcessFactory::loadMissing() is SCORE_TODO/return nullptr. Owned by PR #2179, not by this stack — goes green when #2179 lands"
  "shouldfail@tests/integration/RegressionSplatReloadTest.cpp@Splat's prettyName says Splat, not Model Display@A17: two processes share one display name"
  "shouldfail@tests/unit/AssetTableTest.cpp@AssetTable: zero-byte entries are not reclaimed by trim (current behavior)@trim() skips zero-byte entries, so a table of them never shrinks"
  "shouldfail@tests/gfx/CroustiCpuNodes.cpp@a geometry filter displaces the mesh it is given@P2-9: the CPU geometry-filter path does not displace"
  "shouldfail@tests/gfx/GfxGeometryFilterShift.cpp@a geometry filter shifts the drawn silhouette by exactly the delta@P2-9 oracle, pixel form: the silhouette is not displaced"
  "shouldfail@tests/threedim/SceneApproximationPins.cpp@DEFECT P2-11: the render-thread light encoder collapses area lights onto point, and dome onto directional@light-type information is lost in the render-thread encoder"
  "shouldfail@tests/threedim/SceneApproximationPins.cpp@DEFECT P2-12 (re-scoped): SceneFilterNode mode 2 has no Name port, so it cannot be configured at all@mode 2 exposes no Name port"
  "shouldfail@tests/integration/ThreedimRenderTest.cpp@a model in front of the camera is visible under every Camera projection@P2-3: TWO defects in the four fulldome snippets in ModelDisplayNode.cpp, and fixing either alone leaves this red. (a) theta comes from view-space +Z -- acos(d.z/r) at :124/156/186/216 -- while lookAt looks down -Z, so they image the hemisphere BEHIND the camera -- a model in front lands at r_ndc ~1.9 (smeared over the whole frame) to ~17 (clipped off it). Orthographic is the one law invariant under theta -> pi-theta, and its frame is byte-identical across the axis flip, which is the proof. (b) the same snippets project in_position.xzy at :116/150/180/210 while esVertex/esNormal/v_n stay unswizzled, so the shading describes a differently-oriented model than the one drawn and no face clears the lit-greater-than-24 oracle. Fix (a) alone and the geometry is correct but ambient-only at max luma 3, still 0 drawn pixels -- fix both and it is 7/7. NOT Vulkan-specific -- identical on OpenGL/NVIDIA, Vulkan/NVIDIA and Vulkan/lavapipe. Note drawnPixels() is a LIT-pixel oracle, so 0 here means unlit, not unrasterised. The four LAWS are correct: the sibling case fits them to 0.2% with the camera reversed, and fixing (a) requires re-pointing that sibling camera at the cube, so a fix is its own task"

  # -- CMake WILL_FAIL ------------------------------------------------------
  # Cannot be a Catch2 tag: the defect aborts, so Catch2 never reports.
  "will_fail@tests/integration/CMakeLists.txt@test_integration_js_rootpath_static@rootPath()'s function-local static caches a dangling reference. ASAN-ONLY -- off ASan the freed read trips Qt's own Q_ASSERT only when the garbage is unlucky (measured 8 red / 2 green in 10 runs), so the entry is WILL_FAIL under -fsanitize=address and DISABLED otherwise"

  # -- [finding] tag, VERIFIED GREEN ----------------------------------------
  # These five were the reason this guard was written: real reproduced defects
  # that presented to ctest as ordinary failures, indistinguishable from a
  # regression, because a [finding] tag enforces nothing. Negative-controlling
  # them under OPEN-10 found that all five now PASS -- OpenGL and Vulkan, on
  # this NVIDIA host, 2026-09-03. So the category is empty: there is no longer
  # any test in the tree that is red on purpose and unenforced. They are kept
  # declared, and keep the word FINDING in their names, so that the history
  # stays attached to the case that carries it.
  "fixed@tests/gfx/IsfFindings.cpp@FINDING isf-multipass-storage-rw final pass renders black@a multipass ISF with a read-write storage buffer rendered its final pass all-black -- now renders the uv gradient, asserted"
  "fixed@tests/gfx/IsfFindings.cpp@FINDING isf-multipass-persistent-ssbo final pass renders black@same shape with a persistent SSBO -- the uv pattern and the per-frame ramp are both back"
  "fixed@tests/gfx/IsfMrtPersistent.cpp@FINDING isf-mrt-persistent-ssbo second attachment / Vulkan binding@the second MRT attachment came back blank and the Vulkan pipeline build hit an invalid descriptor -- both attachments are valid now and Vulkan builds"
  "fixed@tests/gfx/GfxVsaCull.cpp@VSA triangle: front face visible on every backend@R3-N4 -- no winding used to render on both backends at once"
  "fixed@tests/gfx/GfxVsaCull.cpp@VSA triangle: backends agree@R3-N4 -- OpenGL drew the triangle while Vulkan culled it"

  # -- [finding] tag, fixed, kept for the name ------------------------------
  # Green today. Declared so the guard does not report them as untracked, and
  # so that a future reader is not misled by the word FINDING in the name.
  "fixed@tests/gfx/GfxIncrementalFindings.cpp@FINDING add-new-output incremental@was a Vulkan render-pass leak -- fixed, and the leak is now COUNTED by the assertion"
  "fixed@tests/gfx/GfxIncrementalFindings.cpp@FINDING resize after an incremental add@used to SIGSEGV on Vulkan -- fixed, and guarded fork-isolated"
)

# Files whose comments discuss [!shouldfail] without registering one. A comment
# ABOUT the convention is not a claim to be one, so these are not findings — but
# the list has to be explicit, because a banner that says a case is pinned when
# it is not is exactly the bug this guard found in GfxPerLayerDepth.cpp.
set(SCORE_EXPECTED_RED_PROSE_ONLY
  tests/integration/GfxProtocolSettingsTest.cpp
  tests/integration/JsScriptingApiTest.cpp
  tests/integration/JsGraphE2ETest.cpp
  tests/integration/JsSetChannelTest.cpp
  tests/unit/GpuFormatsTest.cpp
  tests/unit/AudioFrameEncoderTest.cpp
  tests/unit/AssetLoaderFailure.cpp
  tests/gfx/GfxDropEmptyNodelistAbort.cpp
  tests/gfx/GfxCameraProjectionPin.cpp
  tests/gfx/GfxIncrementalResizeFork.cpp
  tests/gfx/GfxProcessLibrary.cpp
  tests/gfx/GfxShaderCommands.cpp
  tests/gfx/GfxShaderIncludePath.cpp
  tests/gfx/GfxCameraArrayFaces.cpp
  tests/threedim/MaterialOverrideTest.cpp
  tests/threedim/ScenePayloadTransportTest.cpp
)

function(score_check_expected_red)
  set(_root "${CMAKE_CURRENT_SOURCE_DIR}")
  set(_problems "")
  set(_rows "")

  # ---- what the manifest declares, as counts per (mechanism, file) ---------
  set(_declared_files "")
  foreach(_entry IN LISTS SCORE_EXPECTED_RED)
    string(REPLACE "@" ";" _fields "${_entry}")
    list(GET _fields 0 _mech)
    list(GET _fields 1 _path)
    list(GET _fields 2 _case)
    list(GET _fields 3 _why)

    if(NOT EXISTS "${_root}/${_path}")
      list(APPEND _problems
        "  declared ${_mech} pin in ${_path} -- THE FILE DOES NOT EXIST")
      continue()
    endif()
    list(APPEND _declared_files "${_mech}:${_path}")
    string(APPEND _rows "${_mech}\t${_path}\t${_case}\t${_why}\n")
  endforeach()

  # ---- what the tree actually contains --------------------------------------
  # Only REGISTERED tags count. A TEST_CASE tag string closes with a quote:
  #     "[gfx][assettable][!shouldfail]")
  # Prose in a comment does not, and there is a lot of prose.
  file(GLOB_RECURSE _sources
    "${_root}/tests/*.cpp" "${_root}/tests/*.hpp"
    "${_root}/src/*/tests/*.cpp" "${_root}/src/*/*/tests/*.cpp")

  foreach(_src IN LISTS _sources)
    file(RELATIVE_PATH _rel "${_root}" "${_src}")
    file(READ "${_src}" _text)

    # --- Catch2 [!shouldfail], registered ------------------------------------
    string(REGEX MATCHALL "\\[!shouldfail\\]\"\\)" _sf_hits "${_text}")
    list(LENGTH _sf_hits _n_sf)
    set(_want_sf 0)
    foreach(_d IN LISTS _declared_files)
      if(_d STREQUAL "shouldfail:${_rel}")
        math(EXPR _want_sf "${_want_sf} + 1")
      endif()
    endforeach()
    if(NOT _n_sf EQUAL _want_sf)
      list(APPEND _problems
        "  ${_rel}: ${_n_sf} registered [!shouldfail] tag(s), ${_want_sf} declared in the manifest")
    endif()

    # --- [finding] tag: 'finding' (red) and 'fixed' (green) both wear it -----
    string(REGEX MATCHALL "\\[finding\\]" _fd_hits "${_text}")
    list(LENGTH _fd_hits _n_fd)
    set(_want_fd 0)
    foreach(_d IN LISTS _declared_files)
      if(_d STREQUAL "finding:${_rel}" OR _d STREQUAL "fixed:${_rel}")
        math(EXPR _want_fd "${_want_fd} + 1")
      endif()
    endforeach()
    if(NOT _n_fd EQUAL _want_fd)
      list(APPEND _problems
        "  ${_rel}: ${_n_fd} [finding] tag(s), ${_want_fd} declared in the manifest")
    endif()

    # --- a banner that claims a pin the TEST_CASE does not carry -------------
    # This is the shape that shipped in GfxPerLayerDepth.cpp: a 20-line banner
    # opening "EXPECTED TO FAIL -- [!shouldfail] pin", above a TEST_CASE with no
    # such tag. It therefore failed as an ordinary red, which is precisely the
    # confusion the manifest exists to remove.
    # Narrow on purpose. A bare "expected RED" also occurs in pixel prose --
    # GfxPerLayerDepth.cpp:118 says "the expected RED channel value" about a
    # colour channel -- so the parenthesised form is required, which is how the
    # convention is actually written where it is meant as a verdict.
    if(_text MATCHES "EXPECTED TO FAIL|\\(expected RED\\)")
      set(_is_declared 0)
      foreach(_d IN LISTS _declared_files)
        string(FIND "${_d}" ":${_rel}" _f)
        if(NOT _f EQUAL -1)
          set(_is_declared 1)
        endif()
      endforeach()
      list(FIND SCORE_EXPECTED_RED_PROSE_ONLY "${_rel}" _prose)
      if(_n_sf EQUAL 0 AND NOT _is_declared AND _prose EQUAL -1)
        list(APPEND _problems
          "  ${_rel}: a comment announces the case is expected to fail, but no TEST_CASE in the file carries [!shouldfail] and the manifest does not declare it. Either tag the case or fix the comment -- a banner claiming a pin it does not have is worse than no banner at all.")
      endif()
    endif()
  endforeach()

  # ---- CMake WILL_FAIL -------------------------------------------------------
  file(GLOB_RECURSE _cmakes "${_root}/tests/*/CMakeLists.txt" "${_root}/tests/CMakeLists.txt")
  foreach(_cm IN LISTS _cmakes)
    file(RELATIVE_PATH _rel "${_root}" "${_cm}")
    file(READ "${_cm}" _text)
    string(REGEX MATCHALL "set_tests_properties[^\n]*WILL_FAIL" _wf_hits "${_text}")
    list(LENGTH _wf_hits _n_wf)
    set(_want_wf 0)
    foreach(_d IN LISTS _declared_files)
      if(_d STREQUAL "will_fail:${_rel}")
        math(EXPR _want_wf "${_want_wf} + 1")
      endif()
    endforeach()
    if(NOT _n_wf EQUAL _want_wf)
      list(APPEND _problems
        "  ${_rel}: ${_n_wf} WILL_FAIL registration(s), ${_want_wf} declared in the manifest")
    endif()
  endforeach()

  if(_problems)
    list(JOIN _problems "\n" _report)
    message(FATAL_ERROR
      "The expected-red inventory and the test tree disagree.\n"
      "${_report}\n\n"
      "Every test that is red on purpose must be declared exactly once in "
      "SCORE_EXPECTED_RED in cmake/ScoreExpectedRedGuard.cmake, whichever "
      "mechanism enforces it. That manifest is what makes a failing run "
      "triageable on a platform nobody is watching -- it is why the 11 Windows "
      "d3d11 failures could not be sorted into 'ours' and 'new' by hand.")
  endif()

  file(WRITE "${CMAKE_BINARY_DIR}/expected-red.txt"
"# mechanism\tpath\tcase\twhy
# Generated by score_check_expected_red(). Do not edit; edit the manifest in
# cmake/ScoreExpectedRedGuard.cmake and reconfigure.
${_rows}")

  list(LENGTH SCORE_EXPECTED_RED _n_total)
  message(STATUS
    "score: ${_n_total} expected-red tests declared; inventory written to "
    "${CMAKE_BINARY_DIR}/expected-red.txt")
endfunction()
