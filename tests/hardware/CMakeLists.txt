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

  # Strict counterparts: same harness, but run through the provisioning wrapper
  # that REQUIRES gstreamer/ffmpeg/PipeWire and publishes a virtual Video/Source.
  # These cannot skip, so a missing media stack shows up as a failure instead of
  # silently removing the coverage.
  score_add_media_test(
    NAME       Media_PipewireRoundtrip_input
    EXECUTABLE PipewireRoundtrip
    VIRTUAL_VIDEO
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

# --- PipeWire video input through the application (tests/integration) --------
# The harness above drives InputStream directly, against a producer built to
# match its request exactly and with the link made by hand. This one goes
# through the real app -- device settings, protocol, CameraNode, renderer --
# against the runner's Video/Source, and asserts the producer's colour bars
# come out of the window readback.
#
# Needs a real session: the window readback has no GL under the offscreen QPA.
# Registered without OPTIONAL on purpose, like every other media test here --
# an absent display or media stack must show up as a failure rather than
# silently retiring the only end-to-end coverage of the input path.
if(TARGET test_integration_pipewire_video_input)
  score_add_media_test(
    NAME       Media_PipewireVideoInput
    EXECUTABLE test_integration_pipewire_video_input
    VIRTUAL_VIDEO
    TIMEOUT    600)
endif()

# --- libav decode matrix (score-plugin-media) -------------------------------
# The harness is built in tests/unit (NO_CTEST) and registered here, because it
# is the provisioning wrapper that gives it something to decode: --media
# generates one rawvideo-in-NUT clip per pixel format, at even AND odd sizes,
# plus the malformed inputs the failure paths need.
if(TARGET test_unit_libav_decode)
  score_add_media_test(
    NAME       Media_LibavDecode
    EXECUTABLE test_unit_libav_decode
    MEDIA
    TIMEOUT    600)
endif()

# --- container x codec matrix (score-plugin-media) ---------------------------
# --matrix builds one master clip of solid colour blocks, derives its rgb24
# ground truth from that clip, and encodes the same master into every
# container/codec pair the host's ffmpeg can produce -- plus the malformed
# inputs, the file whose header lies about its geometry, and the streams that
# gain or lose an audio track mid-play. Rows the host cannot build are recorded
# by name in unavailable.txt and reported by the harness, so a smaller matrix is
# visible instead of silently green.
if(TARGET test_unit_video_codec_matrix)
  score_add_media_test(
    NAME       Media_VideoCodecMatrix
    EXECUTABLE test_unit_video_codec_matrix
    MATRIX
    TIMEOUT    900)
endif()
