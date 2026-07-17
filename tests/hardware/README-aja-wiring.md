# Wiring the score-addon-aja roundtrips into ctest (design)

The AJA / DeckLink / Magewell harnesses live in the **score-addon-aja**
repository (`src/addons/score-addon-aja/tests/`: `AJARoundtrip.cpp`,
`DecklinkRoundtrip.cpp`, `MagewellCaptureTest.cpp`, `AJAHdmiOutProbe.cpp`),
which is a separate git repo checked out into the score source tree and built
via `add_subdirectory` when present. Their executables are already created
behind `SCORE_VIDEOIO_BUILD_TESTS` (`OFF` by default) + the per-vendor
`VIDEOIO_HAS_{AJA,DECKLINK,MAGEWELL}` feature checks, but nothing registers
them with ctest.

This document is the ready-to-apply recipe for the addon repo; it depends on
the score-side helper introduced in `cmake/ScoreHardwareTests.cmake` and the
probe scripts in `tests/hardware/` in the score tree.
Do not copy the helper or the probes into the addon repo: the addon always
builds inside a score checkout, so `include(ScoreHardwareTests)` (score's
`cmake/` is on `CMAKE_MODULE_PATH`) and `${SCORE_ROOT_SOURCE_DIR}` are
available at addon configure time.

## Semantics recap

`score_add_hardware_test()` registers the test with `SKIP_RETURN_CODE 77`,
`LABELS "hardware;<vendor>"`, `RUN_SERIAL`, a `TIMEOUT`, and runs it through
`tests/hardware/run-hardware-test.sh`: probe fails -> exit 77 -> ctest
`***Skipped`; probe succeeds -> `exec` the harness (its exit code reaches
ctest unchanged; a hang is killed by the ctest TIMEOUT and is a *failure*,
because on a box where the probe passed the device is really there).

## Exact snippet for score-addon-aja/CMakeLists.txt

Append at the end of the existing test-harness section (after the
`MagewellCaptureTest` block, currently the end of the file):

```cmake
# --- ctest wiring: hardware-gated registration ------------------------------
# Each entry probes for its device at runtime and exits 77 (ctest SKIP) when
# absent, so the suite is safe to run on any machine; the dedicated rigs run
# them for real. Helper + probes come from the score tree:
# cmake/ScoreHardwareTests.cmake and tests/hardware/.
if(SCORE_TESTING AND SCORE_VIDEOIO_BUILD_TESTS)
  include(ScoreHardwareTests)
  set(_hw_probes "${SCORE_ROOT_SOURCE_DIR}/tests/hardware")

  if(TARGET AJARoundtrip)
    # Card-to-card SDI roundtrip: out-device 0 -> in-device 1 (default CLI),
    # so it needs BOTH boards present and SDI-cabled. Probe both device nodes
    # rather than "any AJA card".
    # NOTE: on the AJA rig the harness needs the real X server for NVIDIA GL
    # (see memory: "AJARoundtrip needs DISPLAY=:0"); when ctest runs from an
    # ssh session, either export DISPLAY=:0 or uncomment the ENVIRONMENT line.
    score_add_hardware_test(
      NAME       AJARoundtrip
      EXECUTABLE AJARoundtrip
      VENDOR     aja
      PROBE      "test -c /dev/ajantv20 && test -c /dev/ajantv21"
      ARGS       --seconds 2
      TIMEOUT    1800
      # ENVIRONMENT "DISPLAY=:0"
      )
  endif()

  if(TARGET aja_hdmi_out_probe)
    # TX-side HDMI validation (the Kona 5 has no HDMI input on any
    # personality): single card, SDK-only, no cabling required.
    # --force-hpd brings the TX up without an EDID sink.
    score_add_hardware_test(
      NAME       AJAHdmiOutProbe
      EXECUTABLE aja_hdmi_out_probe
      VENDOR     aja
      PROBE      "${_hw_probes}/probe-aja.sh"
      ARGS       --force-hpd
      TIMEOUT    600)
  endif()

  if(TARGET DecklinkRoundtrip)
    # Single-card loopback; rig cabling: HDMI out -> HDMI in, SDI out -> SDI in
    # (sat-lenovo-video). Probe = DesktopVideo driver nodes, i.e. card present
    # AND driver installed — a bare PCI match is not enough for the DeckLink
    # API to enumerate anything.
    score_add_hardware_test(
      NAME       DecklinkRoundtrip
      EXECUTABLE DecklinkRoundtrip
      VENDOR     decklink
      PROBE      "${_hw_probes}/probe-decklink.sh"
      ARGS       --seconds 2
      TIMEOUT    1800)
  endif()

  if(TARGET MagewellCaptureTest)
    # Live-capture validation. Default CLI captures channels 0,2; on the rig
    # those are fed by the AJA HDMI outs (Magewell HDMI 1/3 <- AJA HDMI outs),
    # so a PASS additionally depends on that cabling + an AJA source running.
    score_add_hardware_test(
      NAME       MagewellCaptureTest
      EXECUTABLE MagewellCaptureTest
      VENDOR     magewell
      PROBE      "${_hw_probes}/probe-magewell.sh"
      ARGS       --seconds 2
      TIMEOUT    900)
  endif()
endif()
```

## Probes used

| Harness            | Probe                                              | Rationale |
|--------------------|----------------------------------------------------|-----------|
| AJARoundtrip       | inline `test -c /dev/ajantv20 && test -c /dev/ajantv21` | roundtrip needs two boards (out=0/in=1) |
| AJAHdmiOutProbe    | `tests/hardware/probe-aja.sh` (any `/dev/ajantv2*` char dev) | single card suffices |
| DecklinkRoundtrip  | `tests/hardware/probe-decklink.sh` (`/dev/blackmagic/*` or `/sys/module/blackmagic`) | DesktopVideo driver + card |
| MagewellCaptureTest| `tests/hardware/probe-magewell.sh` (PCI vendor `0x1cd7` via sysfs, lspci fallback) | card presence |

## Operational notes

- Run only the vendor subset on a rig: `ctest -L aja`, `ctest -L decklink`,
  `ctest -L magewell`; exclude everywhere: `ctest -LE hardware`.
- All four are `RUN_SERIAL` (exclusive device channels; AJA cards cannot be
  shared between the roundtrip and the HDMI probe concurrently).
- `--seconds 2` keeps the full AJA format/pixfmt matrix within the 1800 s
  budget; drop `--formats`/`--pixfmt` filters in if a shorter smoke run is
  wanted for CI.
- Attribution: this snippet (and only it) belongs to the **score-addon-aja**
  repo; the helper +
  probe scripts belong to the score tree.
