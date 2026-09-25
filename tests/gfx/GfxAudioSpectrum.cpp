// Pins the audioFFT and audioHistogram textures of an ISF node for audio
// buffers whose length is not a power of two (the dummy audio backend delivers
// ~441-sample ticks): every texel is finite, the texture spans DC to nyquist
// (width = next power of two / 2), a sine peaks at the expected bin, and the
// histogram rows stay stable when a waveform input shares the node.
#include "IsfTestCommon.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
ossia::audio_vector sine(double cyclesPerSample, int samples, int channels = 1)
{
  ossia::audio_vector v;
  v.resize(channels);
  for(auto& c : v)
  {
    c.resize(samples);
    for(int i = 0; i < samples; ++i)
      c[i] = std::sin(2. * M_PI * cyclesPerSample * i);
  }
  return v;
}

void poisonHeap()
{
  std::vector<void*> blocks;
  for(std::size_t bytes : {std::size_t(4104), std::size_t(8200), std::size_t(2056)})
    for(int i = 0; i < 256; ++i)
    {
      void* p{};
      if(::posix_memalign(&p, 32, bytes) == 0)
      {
        std::memset(p, 0xff, bytes);
        blocks.push_back(p);
      }
    }
  for(void* p : blocks)
    std::free(p);
}

IsfResult renderAudio(
    score::gfx::GraphicsApi backend, const QString& path,
    const std::vector<std::pair<int, ossia::audio_vector>>& audio, QSize size,
    int frames)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r.backend = backend_name(backend);
    GfxPipeline p;
    const int n = p.addIsf(path);
    if(n < 0)
    {
      r.error = p.error();
      return;
    }
    const int s = p.addSink(size);
    p.wire(p.imageOut(n, 0), p.sinkInput(s));
    if(!p.create(backend))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    poisonHeap();
    for(auto& [port, buf] : audio)
      setAudio(*p.isf(n), port, buf);
    p.render(frames);
    ReadbackImage img = p.readback(s);
    if(!img.valid())
      r.error = "audio readback empty";
    r.outputs.push_back(std::move(img));
  });
  return r;
}

struct Row
{
  int width{};
  int nonFinite{};
  int peak{-1};
  int peakValue{};
  std::vector<int> values;
};

Row scanRow(const ReadbackImage& img, int y)
{
  Row row;
  for(int x = 0; x < img.width; ++x)
  {
    const auto p = img.at(x, y);
    if(p[2] > 127)
      row.width++;
    if(p[1] > 127)
      row.nonFinite++;
    row.values.push_back(p[0]);
    if(p[0] > row.peakValue)
    {
      row.peakValue = p[0];
      row.peak = x;
    }
  }
  return row;
}
}

TEST_CASE(
    "ISF audioFFT texture is finite and peaks at the sine's bin",
    "[gfx][isf][audio][fft]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const int samples = GENERATE(441, 512);
  const double bin = GENERATE(32.0, 230.0);
  CAPTURE(backend_name(backend), samples, bin);

  const IsfResult r = renderAudio(
      backend, corpus("isf-audio-fft-bins.fs"), {{0, sine(bin / 512., samples)}},
      {512, 4}, 4);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  for(int y = 0; y < r.outputs[0].height; ++y)
  {
    const Row row = scanRow(r.outputs[0], y);
    CAPTURE(y);
    CHECK(row.nonFinite == 0);
    CHECK(row.width == 256);
    CHECK(std::abs(row.peak - int(bin)) <= 1);
    CHECK(row.peakValue > 100);
  }
}

TEST_CASE(
    "ISF audioHistogram texture is finite, spans nyquist and keeps its rows "
    "next to a waveform input",
    "[gfx][isf][audio][histogram]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const double bin = GENERATE(32.0, 230.0);
  CAPTURE(backend_name(backend), bin);

  const auto buf = sine(bin / 512., 441);
  const IsfResult r = renderAudio(
      backend, corpus("isf-audio-hist-rows.fs"), {{0, buf}, {1, buf}}, {512, 2},
      4);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const Row a = scanRow(r.outputs[0], 0);
  const Row b = scanRow(r.outputs[0], 1);
  for(const Row* row : {&a, &b})
  {
    CHECK(row->nonFinite == 0);
    CHECK(row->width == 254);
    CHECK(std::abs(row->peak - (int(bin) - 1)) <= 2);
    CHECK(row->peakValue > 200);
  }
  int maxDiff = 0;
  for(int x = 0; x < 254; ++x)
    maxDiff = std::max(maxDiff, std::abs(a.values[x] - b.values[x]));
  CHECK(maxDiff <= 2);
}
