// =============================================================================
// L3 ISF feature surface — EXPLICIT COVERAGE GAPS (honest SKIPs).
//
// These ISF inputs cannot be driven by the current render+readback fixture
// (tests/fixtures/score_test/Gfx.hpp), which can only wire IMAGE producers into
// image inputs and pump frames. Each case SKIPs with a precise reason so the gap
// is visible in the test report instead of being silently absent. Closing any of
// these needs a documented fixture EXTENSION (owned by another effort).
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

// A tiny non-skipped anchor so the target reports a run assertion rather than
// Catch2's "no tests ran" (exit 4) when every real case below is a SKIP.
TEST_CASE("ISF unsupported-input coverage manifest", "[gfx][l3][isf][skip]")
{
  // The families enumerated below are explicit, documented coverage gaps.
  CHECK(true);
}

TEST_CASE(
    "isf-audiofft / audiohist need an FFT/histogram source",
    "[gfx][l3][isf][skip][audio]")
{
  // UPDATE: the TEMPORAL TYPE:"audio" family is now COVERED — the fixture gained
  // setAudio()/render_isf_audio() (Gfx.hpp), which feed a known ossia::audio_vector
  // into the node's audio inlet through the same ProcessNode::process(port,
  // audio_vector) path the exec engine uses; a constant buffer is analytic
  // (texel = 0.5 + s/2). See test_gfx_audio (GfxAudio.cpp).
  SKIP("isf-audiofft-input.fs / isf-audiohist-input.fs declare TYPE:\"audioFFT\"/"
       "\"audioHist\" inputs whose textures are the FFT / histogram of the audio "
       "buffer (processSpectral / processHistogram). setAudio() can feed the raw "
       "buffer, but the shader output is a spectral/histogram transform, not an "
       "analytic function of the input, so it is not asserted here. (TEMPORAL "
       "TYPE:\"audio\" IS covered — see test_gfx_audio.)");
}

TEST_CASE(
    "isf-cubemap-input needs a cubemap texture input", "[gfx][l3][isf][skip][cubemap]")
{
  SKIP("isf-cubemap-input.fs declares a samplerCube image input. The fixture "
       "wires plain 2D image producers only; there is no path to feed a cubemap "
       "(6-face) texture. Needs a fixture extension providing a cubemap source.");
}

TEST_CASE(
    "isf-all-input-types needs control-value + cubemap/3D injection",
    "[gfx][l3][isf][skip][inputs]")
{
  // UPDATE: control-value injection now EXISTS (setControl/render_isf_controls in
  // Gfx.hpp; see test_gfx_control). The only part of isf-all-input-types.fs still
  // undrivable is the non-2D texture inputs (cubemap / 3D) and audioFFT/audioHist.
  SKIP("isf-all-input-types.fs exercises the full ISF INPUTS matrix "
       "(float/color/point2D/bool/long/event controls plus image/audio/cubemap). "
       "The control-value AND temporal-audio parts are now injectable (see "
       "test_gfx_control / test_gfx_audio), but the cubemap and 3D texture inputs "
       "still have no producer path, so this whole-matrix shader cannot be driven "
       "end-to-end. Needs a cubemap/3D texture source.");
}

TEST_CASE(
    "ISF control-value inputs (float/color/point/bool/long) ARE now covered",
    "[gfx][l3][isf][inputs]")
{
  // Previously a SKIP. The fixture now injects control values through the same
  // material-UBO path the exec engine drives (setControl -> ProcessNode::process
  // (port, ossia::value) -> material slot + materialChange). float / color /
  // point2D / point3D / bool / long are covered analytically per backend in
  // test_gfx_control (GfxControl.cpp). This anchor records that the gap closed.
  //
  // Remaining control gap: TYPE:"event" — an int pulse that ISFNode resets to 0
  // after each frame (resetEventPortsAfterFrame / m_event_ports), so asserting it
  // needs a per-frame injection cadence rather than a set-once value; not yet
  // exercised here.
  CHECK(true);
}

TEST_CASE(
    "ISF uniform_input (camera UBO) shaders need an upstream Buffer producer",
    "[gfx][l3][isf][skip][uniform]")
{
  SKIP("isf-mrt-uniform-input.fs, isf-multipass-uniform-input.fs and "
       "isf-persistent-uniform-input.fs consume a TYPE:\"uniform\" camera UBO fed "
       "from an upstream CameraUBOBuilder / Buffer port. The fixture builds ISF "
       "image chains only and cannot attach a Buffer producer, so the camera UBO "
       "stays a zero placeholder and the uniform-driven output cannot be "
       "verified. Needs a fixture extension that wires a known UBO into the "
       "node's uniform_input port.");
}

TEST_CASE(
    "feedback-texture cross-node self-feedback needs a Delayed edge",
    "[gfx][l3][isf][skip][feedback]")
{
  SKIP("feedback-texture.fs expects its OWN output wired back to its 'prev' image "
       "input via a Delayed feedback edge (a 1-frame cycle). The linear-chain "
       "fixture cannot build a self-loop / delayed edge, so the cross-node "
       "texture feedback cannot be exercised. (Single-node PERSISTENT feedback IS "
       "covered by isf-persistent-feedback.fs in IsfPersistent.cpp.) Needs a "
       "fixture extension for delayed/self edges.");
}
