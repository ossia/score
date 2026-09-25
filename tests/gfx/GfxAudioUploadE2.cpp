// Pins two properties of the ISF audio texture upload:
//  - a waveform input with an odd sample count gets a texture exactly that wide,
//    every column holding a sample (the width was rounded up to even and the
//    last column never uploaded);
//  - the upload runs once per frame however many output edges the node has, so
//    a histogram node feeding two inputs adds one row per frame, not two.
#include "IsfTestCommon.hpp"

#include <cmath>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
ossia::audio_vector sine(double cyclesPerSample, int samples)
{
  ossia::audio_vector v;
  v.resize(1);
  v[0].resize(samples);
  for(int i = 0; i < samples; ++i)
    v[0][i] = std::sin(2. * M_PI * cyclesPerSample * i);
  return v;
}

struct HistRun
{
  IsfResult result;
  int litRows{-1};
};

HistRun renderHistogramRows(score::gfx::GraphicsApi backend, bool twoEdges, int frames)
{
  HistRun run;
  auto& r = run.result;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r.backend = backend_name(backend);
    GfxPipeline p;
    const int h = p.addIsf(corpus("e2-audio-hist-row-levels.fs"));
    const int m = twoEdges ? p.addIsf(corpus("isf-mix-two.fs")) : -1;
    if(h < 0 || (twoEdges && m < 0))
    {
      r.error = p.error();
      return;
    }
    const int s = p.addSink({240, 1});
    if(twoEdges)
    {
      p.wire(p.imageOut(h, 0), p.imageIn(m, 0));
      p.wire(p.imageOut(h, 0), p.imageIn(m, 1));
      p.wire(p.imageOut(m, 0), p.sinkInput(s));
    }
    else
    {
      p.wire(p.imageOut(h, 0), p.sinkInput(s));
    }
    if(!p.create(backend))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    setAudio(*p.isf(h), 0, sine(32. / 512., 512));
    p.render(frames);
    ReadbackImage img = p.readback(s);
    if(!img.valid())
    {
      r.error = "histogram readback empty";
      return;
    }
    run.litRows = 0;
    for(int x = 0; x < img.width; ++x)
      if(img.at(x, 0)[0] > 64)
        run.litRows++;
    r.outputs.push_back(std::move(img));
  });
  return run;
}
}

TEST_CASE(
    "ISF waveform texture of an odd sample count holds every sample",
    "[gfx][isf][audio][waveform]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const int samples = GENERATE(441, 255);
  CAPTURE(backend_name(backend), samples);

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_audio(
        backend, corpus("e2-audio-wave-columns.fs"), 0, const_audio(1.0, samples),
        {}, {512, 2}, 3);
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const ReadbackImage& img = r.outputs[0];

  int width = 0;
  int wrong = 0;
  for(int x = 0; x < img.width; ++x)
  {
    const auto px = img.at(x, 0);
    if(px[2] > 127)
    {
      width++;
      if(px[0] < 250)
        wrong++;
    }
  }
  CHECK(width == samples);
  CHECK(wrong == 0);
}

TEST_CASE(
    "ISF audio histogram adds one row per frame with two output edges",
    "[gfx][isf][audio][histogram]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const int frames = 3;
  const HistRun one = renderHistogramRows(backend, false, frames);
  if(one.result.skipped)
    SKIP(one.result.backend + ": " + one.result.skip_reason);
  REQUIRE(one.result.error.empty());
  const HistRun two = renderHistogramRows(backend, true, frames);
  REQUIRE(two.result.error.empty());

  CAPTURE(one.litRows, two.litRows);
  CHECK(one.litRows >= 1);
  CHECK(one.litRows <= frames);
  CHECK(two.litRows == one.litRows);
}
