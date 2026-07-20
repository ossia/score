// =============================================================================
// L3 ISF AUDIO input injection (Priority 2 inputs, secondary).
//
// An ISF TYPE:"audio" INPUT becomes a sampler2D (X=sample, Y=channel) the node
// owns as an AudioTexture. The exec engine pushes audio as an ossia::audio_vector;
// here the fixture's setAudio() / render_isf_audio() drive the SAME public entry
// point (ProcessNode::process(port, audio_vector)), and the renderer's
// updateAudioTexture uploads it into an R32F texture. For the TEMPORAL path a
// sample s is stored as texel 0.5 + s/2, so a CONSTANT single-channel buffer of
// value s makes every texel 0.5 + s/2 — analytic through isf-audio-const.fs
// (which emits the sampled value as grayscale). readback == round(255*(0.5+s/2)).
//
// This closes the "isf-audio-input needs an audio source" SKIP for the TEMPORAL
// audio family. audioFFT / audioHist run an FFT / histogram over the buffer and
// are NOT analytic here (documented in IsfUnsupported.cpp).
//
// Run: DISPLAY=:0 ctest -R gfx_audio --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
IsfResult audio_render(
    score::gfx::GraphicsApi backend, const QString& path, int audioPort,
    const ossia::audio_vector& audio)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_audio(backend, path, audioPort, audio);
  });
  return r;
}

int expect_level(double s) // 0.5 + s/2 -> [0,255]
{
  return int(255.0 * (0.5 + s / 2.0) + 0.5);
}

void check_uniform(const ReadbackImage& img, int level, int tol)
{
  REQUIRE(img.valid());
  for(int y = 4; y < img.height - 4; y += 8)
    for(int x = 4; x < img.width - 4; x += 8)
    {
      const auto p = img.at(x, y);
      INFO("pixel (" << x << "," << y << ") r=" << (int)p[0] << " expect " << level);
      CHECK(std::abs(int(p[0]) - level) <= tol);
      CHECK(std::abs(int(p[1]) - level) <= tol);
      CHECK(std::abs(int(p[2]) - level) <= tol);
      CHECK(p[3] == 255);
    }
}
} // namespace

TEST_CASE("ISF temporal audio input is injectable and analytic", "[gfx][l3][isf][audio]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // Constant 0.0 -> texel 0.5 -> gray 128.
  const IsfResult r = audio_render(
      backend, corpus("isf-audio-const.fs"), 0, const_audio(0.0, 256));
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  check_uniform(r.outputs[0], expect_level(0.0), 3); // 128
}

TEST_CASE("ISF temporal audio input sweeps analytically", "[gfx][l3][isf][audio]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult hi = audio_render(
      backend, corpus("isf-audio-const.fs"), 0, const_audio(1.0, 256));
  if(hi.skipped)
    SKIP(hi.backend + ": " + hi.skip_reason);
  const IsfResult lo = audio_render(
      backend, corpus("isf-audio-const.fs"), 0, const_audio(-1.0, 256));
  const IsfResult mid = audio_render(
      backend, corpus("isf-audio-const.fs"), 0, const_audio(0.5, 256));
  REQUIRE(hi.error.empty());
  REQUIRE(lo.error.empty());
  REQUIRE(mid.error.empty());

  check_uniform(hi.outputs[0], expect_level(1.0), 3);   // 255
  check_uniform(lo.outputs[0], expect_level(-1.0), 3);  // 0
  check_uniform(mid.outputs[0], expect_level(0.5), 3);  // 191
}

TEST_CASE("ISF temporal audio input agrees across backends", "[gfx][l3][isf][audio]")
{
  std::vector<ReadbackImage> got;
  for(auto api : platform_backends())
  {
    const IsfResult r = audio_render(
        api, corpus("isf-audio-const.fs"), 0, const_audio(0.5, 256));
    if(!r.skipped && r.error.empty() && !r.outputs.empty() && r.outputs[0].valid())
      got.push_back(r.outputs[0]);
  }
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 3);
  }
}
