# Hardware-gated round-trip tests.
#
# Every entry here is registered unconditionally (when its harness target got
# built) and probes for its device at runtime: on the dedicated rigs the test
# runs for real, everywhere else it exits 77 -> ctest "***Skipped".
#
#   ctest -L hardware        # only hardware tests
#   ctest -LE hardware       # everything except hardware tests
#   ctest -L pipewire        # per-vendor subset
#
# The AJA / DeckLink / Magewell roundtrips live in the score-addon-aja repo
# (src/addons/score-addon-aja/tests/); their registration belongs in that
# repo's CMakeLists.txt using the same helper — see README-aja-wiring.md.

include(ScoreHardwareTests)

# --- PipeWire video round-trip (src/plugins/score-plugin-gfx) ----------------
# Two entries with different gates:
#  - PipewireRoundtrip_input: raw libpipewire producer -> score InputStream in
#    every input format score claims to support (+ padded strides). Pure
#    sysmem/negotiation paths, deterministic wherever a live daemon runs —
#    including headless boxes (the wrapper falls back to offscreen).
#  - PipewireRoundtrip_s2s: score TexgenNode -> PipewireOutputNode -> score
#    InputStream over SHM/dma-buf on GL+Vulkan. Renders through the real GPU;
#    llvmpipe/offscreen can never pass the PSNR gate (verified: 5 dB, 780 ms
#    latency, frame repeats), so this one is additionally display-gated and
#    only runs on rigs with a real session.
#
# KNOWN FAILURE until the libossia submodule advances libremidi past 91ecbd5
# ("pipewire: version-robust invoke_sync"): older libremidi calls a blocking
# pw_loop_invoke without the thread-loop lock, so the score PipeWire input's
# stop() deadlocks after the first verified cell and ctest kills the test at
# TIMEOUT. Verified: the identical harness built against the fixed libremidi
# (wt/score-interop) runs the full input matrix to 14/14 PASS, exit 0.
# Exclude with `ctest -LE hardware` (or -LE pipewire) in loops that must stay
# green meanwhile.
if(TARGET PipewireRoundtrip)
  # Sanitizer-build friendly; inert on plain builds. halt_on_error=0 keeps
  # the full matrix report even if ASAN flags something mid-run.
  set(_pwrt_asan
    "ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0")

  score_add_hardware_test(
    NAME       PipewireRoundtrip_input
    EXECUTABLE PipewireRoundtrip
    VENDOR     pipewire
    PROBE      "${CMAKE_CURRENT_SOURCE_DIR}/probe-pipewire.sh"
    ARGS       --seconds 2 --only pw2s
    TIMEOUT    600
    ENVIRONMENT "${_pwrt_asan}")

  score_add_hardware_test(
    NAME       PipewireRoundtrip_s2s
    EXECUTABLE PipewireRoundtrip
    VENDOR     pipewire
    PROBE      "${CMAKE_CURRENT_SOURCE_DIR}/probe-pipewire.sh &&\
 ${CMAKE_CURRENT_SOURCE_DIR}/probe-display.sh"
    ARGS       --seconds 2 --only s2s
    TIMEOUT    600
    ENVIRONMENT "${_pwrt_asan}")
endif()
