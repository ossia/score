// The Gray code + phase shift decoder on a camera that sees the projector pixel
// for pixel: frames in any order, missing or repeated frames, a coarse bit on
// a stripe edge, clipped pixels.

#include <Gfx/Calibration/PairCode.hpp>
#include <Threedim/CameraLens.hpp>
#include <Threedim/StructuredLight.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

namespace sl = Threedim::sl;

namespace
{
constexpr int PW = 256, PH = 64;

//! Camera pixel (x, y) sees projector pixel (x, y): light 0.05 + 0.8 value.
std::vector<float> frame(const sl::GraySequence& s, int k)
{
  std::vector<float> f(std::size_t(PW) * PH);
  for(int y = 0; y < PH; y++)
    for(int x = 0; x < PW; x++)
      f[std::size_t(y) * PW + x] = 0.05f + 0.8f * s.value(k, x, y);
  return f;
}

sl::GraySequence sequence(bool phase)
{
  auto s = sl::GraySequence::make(PW, PH, 1);
  if(phase)
    s.enablePhase(4, 0, 1.f);
  return s;
}
}

TEST_CASE("Gray sequence order, and thin stripes skipped", "[threedim][structuredlight]")
{
  const auto s = sl::GraySequence::make(1920, 1080, 1);
  CHECK(s.bitsX == 11);
  CHECK(s.bitsY == 11);
  CHECK(s.count() == 2 + 2 * 22);
  CHECK(s.lit(0, 5, 5));
  CHECK_FALSE(s.lit(1, 5, 5));
  for(int u : {0, 1, 700, 1919})
  {
    const int g = u ^ (u >> 1);
    CHECK(s.lit(2, u, 0) == bool((g >> 10) & 1));
    CHECK(s.lit(3, u, 0) != s.lit(2, u, 0));
    CHECK(s.lit(2 + 2 * 10, u, 0) == bool(g & 1));
  }
  const int v = 777, g = v ^ (v >> 1);
  CHECK(s.lit(2 + 2 * 11, 0, v) == bool((g >> 10) & 1));

  const auto t = sl::GraySequence::make(1920, 1080, 8);
  CHECK(t.skip == 2);
  CHECK(t.count() == 2 + 2 * (9 + 9));
}

TEST_CASE("The decoder reads every projector pixel back, in any frame order", "[threedim][structuredlight]")
{
  const auto s = sequence(true);
  std::vector<int> order(s.count());
  std::iota(order.begin(), order.end(), 0);
  const bool shuffled = GENERATE(false, true);
  if(shuffled)
    std::shuffle(order.begin(), order.end(), std::mt19937{7});

  sl::Decoder d;
  d.reset(PW, PH, s);
  for(int k : order)
    REQUIRE(d.add(k, frame(s, k)));
  CHECK(d.missing().empty());
  const auto px = d.decode(0.1f, 6);
  int bad = 0, phase = 0;
  for(int y = 0; y < PH; y++)
    for(int x = 0; x < PW; x++)
    {
      const auto& p = px[std::size_t(y) * PW + x];
      if(!p.valid || std::abs(p.u - x) > 0.05f || std::abs(p.v - y) > 0.05f)
        bad++;
      phase += p.phaseU && p.phaseV;
    }
  CHECK(bad == 0);
  CHECK(phase == PW * PH);
}

TEST_CASE("The decoder refuses frames it cannot place and decodes nothing short of one", "[threedim][structuredlight]")
{
  const auto s = sequence(false);
  sl::Decoder d;
  d.reset(PW, PH, s);
  for(int k = 0; k < s.count(); k++)
    if(k != 5)
      REQUIRE(d.add(k, frame(s, k)));
  CHECK(d.missing() == std::vector<int>{5});
  const auto none = d.decode(0.1f, 6);
  CHECK(std::none_of(none.begin(), none.end(), [](auto& p) { return p.valid; }));

  CHECK_FALSE(d.add(4, frame(s, 4)));
  CHECK(d.error().find("twice") != std::string::npos);
  CHECK_FALSE(d.add(s.count(), frame(s, 0)));
  CHECK_FALSE(d.add(5, std::vector<float>(10)));
  CHECK(d.missing() == std::vector<int>{5});
  REQUIRE(d.add(5, frame(s, 5)));
  const auto all = d.decode(0.1f, 6);
  CHECK(std::all_of(all.begin(), all.end(), [](auto& p) { return p.valid; }));
}

TEST_CASE("A pixel on the edge of a coarse stripe keeps its finer bits", "[threedim][structuredlight]")
{
  const auto s = sequence(false);
  sl::Decoder d;
  d.reset(PW, PH, s);
  for(int k = 0; k < s.count(); k++)
  {
    auto f = frame(s, k);
    const float edge = 0.5f * (0.05f + 0.8f * s.value(k, 127, 0) + 0.05f + 0.8f * s.value(k, 128, 0));
    f[0] = edge;
    REQUIRE(d.add(k, f));
  }
  const auto px = d.decode(0.1f, s.projectedX());
  REQUIRE(px[0].valid);
  CHECK(px[0].u >= 126.5f);
  CHECK(px[0].u <= 128.5f);
}

TEST_CASE("A clipped pixel decodes its Gray code and not its phase", "[threedim][structuredlight]")
{
  const auto s = sequence(true);
  sl::Decoder d;
  d.reset(PW, PH, s);
  std::vector<uint8_t> clipped(std::size_t(PW) * PH, 0);
  clipped[3 * PW + 40] = 1;
  d.markSaturated(clipped);
  for(int k = 0; k < s.count(); k++)
    REQUIRE(d.add(k, frame(s, k)));
  const auto px = d.decode(0.1f, 6);
  const auto& p = px[3 * PW + 40];
  CHECK(p.saturated);
  CHECK(p.valid);
  CHECK_FALSE(p.phaseU);
  CHECK(std::abs(p.u - 40.f) <= 1.f);
  CHECK(px[3 * PW + 41].phaseU);
}

TEST_CASE("Pattern pairs read their bit against the pixel's span", "[threedim][structuredlight]")
{
  namespace pc = score::gfx::paircode;
  const auto a = pc::read(0.6, 0.2, 0.8, 0.25);
  CHECK(a.one);
  CHECK(a.clear);
  CHECK(a.contrast == 0.5f);
  const auto b = pc::read(0.41, 0.4, 0.8, 0.25);
  CHECK(b.one);
  CHECK_FALSE(b.clear);
  CHECK_FALSE(pc::read(0.2, 0.6, 0.8, 0.25).one);
}

TEST_CASE("Camera lens models project their own unprojection back", "[threedim][structuredlight][lens]")
{
  using Threedim::CameraLens;
  const std::array<double, 4> K{400., 410., 319.5, 239.5};
  for(auto [model, k] : std::initializer_list<std::pair<CameraLens::Model, std::array<double, 5>>>{
          {CameraLens::Pinhole, {}},
          {CameraLens::Distorted, {-0.12, 0.03, 0.001, -0.0008, 0.002}},
          {CameraLens::Fisheye, {0.05, -0.01, 0.002, 0.}}})
  {
    CameraLens lens;
    lens.model = model;
    lens.k = k;
    CAPTURE(CameraLens::name(model));
    CHECK(CameraLens::fromName(CameraLens::name(model)) == model);
    double worst = 0.;
    for(double py : {10., 120., 239.5, 400., 470.})
      for(double px : {5., 160., 319.5, 500., 630.})
      {
        const auto d = lens.unproject(K, px, py);
        double qx{}, qy{};
        REQUIRE(lens.project(K, d, qx, qy));
        worst = std::max({worst, std::abs(qx - px), std::abs(qy - py)});
      }
    CHECK(worst < 1e-3);
  }
}

TEST_CASE("Push-pull fills a hole from one side of a depth edge", "[threedim][structuredlight]")
{
  sl::SparseMap m;
  m.reset(16, 8);
  for(int v = 0; v < 8; v++)
    for(int u = 0; u < 16; u++)
    {
      if(u >= 6 && u < 10)
        continue;
      m.keepBest(u, v, {0.f, 0.f, u < 8 ? 1.f : 3.f}, 1.f);
    }
  const auto averaged = sl::pushPullFill(m, 8, 0.f, false);
  const auto sided = sl::pushPullFill(m, 8, 0.5f, false);
  int between = 0, midAir = 0;
  for(int v = 0; v < 8; v++)
    for(int u = 6; u < 10; u++)
    {
      const std::size_t i = (std::size_t(v) * 16 + u) * 4;
      REQUIRE(sided[i + 3] == 1.f);
      const float z = sided[i + 2];
      midAir += z > 1.25f && z < 2.75f;
      const float za = averaged[i + 2];
      between += za > 1.25f && za < 2.75f;
    }
  CHECK(midAir == 0);
  CHECK(between > 0);
  const auto none = sl::pushPullFill(m, 1, 0.5f, false);
  CHECK(none[(3 * 16 + 7) * 4 + 3] == 0.f);
}
