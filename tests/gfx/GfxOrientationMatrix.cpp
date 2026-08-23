// =============================================================================
// Orientation and NDC convention, across chain depth and across backends.
//
// GfxOrientation.cpp pins the vertical axis of a SINGLE node against its own
// analytic ramp. This file pins the property that matters to a patch author:
// every path that carries a picture must carry it the same way up, whatever
// backend renders it and however many nodes it passes through.
//
//   source                          -> readback
//   source -> passthrough           -> readback
//   source -> passthrough -> passthrough -> readback
//
// must all deliver the same image. isf-passthrough-plain.fs is a verbatim
// IMG_NORM_PIXEL fetch, so any difference between those three is the engine's
// intermediate-texture convention, not the shader's.
//
// The oracle is a four-quadrant colour key rather than a ramp or a corner
// marker: a ramp cannot distinguish a mirror along its constant axis, and a
// single marker cannot distinguish a transpose from a rotation. Absolute
// orientation is asserted at every depth as well as agreement between depths,
// so "all three agree" cannot pass by all three being wrong the same way.
//
//   DISPLAY=:0 ctest -R gfx_orientation_matrix
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null
// backend, which produces a stable, self-consistent and completely wrong
// picture.
// =============================================================================

#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Wider than GfxOrientation.cpp's kTol: a quadrant average tolerates an
// intermediate texture's filtering, where a per-row ramp comparison does not.
constexpr int kQuadTol = 24;
// Same budget the per-node ramp assertions use.
constexpr int kRampTol = 8;

struct RampFit
{
  int worst{};
  int worstRow{};
  int got{};
  int expected{};
};

/// Green channel against the closed-form ISF ramp: 255 at row 0, 0 at the last
/// row, sampled at fragment centres.
RampFit fit_vertical_ramp(const ReadbackImage& img)
{
  RampFit f;
  for(int y = 0; y < img.height; ++y)
  {
    const double t = (double(y) + 0.5) / double(img.height);
    const int expected = int(std::lround(255.0 * (1.0 - t)));
    for(int x : {1, img.width / 2, img.width - 2})
    {
      const int got = img.at(x, y)[1];
      const int d = std::abs(got - expected);
      if(d > f.worst)
        f = {d, y, got, expected};
    }
  }
  return f;
}

struct Quadrants
{
  std::array<uint8_t, 4> tl{}, tr{}, bl{}, br{};
};

/// Average each quadrant well inside its bounds, away from the seams where
/// filtering of an intermediate texture blends neighbours.
Quadrants sample_quadrants(const ReadbackImage& img)
{
  const int w = img.width, h = img.height;
  auto avg = [&](int x0, int y0, int x1, int y1) {
    long acc[4] = {0, 0, 0, 0};
    int n = 0;
    for(int y = y0; y < y1; ++y)
      for(int x = x0; x < x1; ++x, ++n)
      {
        const auto p = img.at(x, y);
        for(int c = 0; c < 4; ++c)
          acc[c] += p[c];
      }
    std::array<uint8_t, 4> out{};
    for(int c = 0; c < 4; ++c)
      out[c] = uint8_t(n ? acc[c] / n : 0);
    return out;
  };
  const int xa = w / 8, xb = 3 * w / 8, xc = 5 * w / 8, xd = 7 * w / 8;
  const int ya = h / 8, yb = 3 * h / 8, yc = 5 * h / 8, yd = 7 * h / 8;
  return {avg(xa, ya, xb, yb), avg(xc, ya, xd, yb), avg(xa, yc, xb, yd),
          avg(xc, yc, xd, yd)};
}

bool is_colour(std::array<uint8_t, 4> got, int r, int g, int b)
{
  return std::abs(int(got[0]) - r) <= kQuadTol && std::abs(int(got[1]) - g) <= kQuadTol
         && std::abs(int(got[2]) - b) <= kQuadTol;
}

std::string describe(const Quadrants& q)
{
  auto one = [](std::array<uint8_t, 4> c) {
    return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
           + std::to_string(c[2]) + ")";
  };
  return "TL=" + one(q.tl) + " TR=" + one(q.tr) + " BL=" + one(q.bl)
         + " BR=" + one(q.br);
}

/// Name the transform that maps the expected key onto what arrived, so a
/// failure says "vertical flip" instead of printing four colours.
std::string diagnose(const Quadrants& q)
{
  const bool tl_r = is_colour(q.tl, 255, 0, 0), tr_g = is_colour(q.tr, 0, 255, 0);
  const bool bl_b = is_colour(q.bl, 0, 0, 255), br_w = is_colour(q.br, 255, 255, 255);
  if(tl_r && tr_g && bl_b && br_w)
    return "identity";
  if(is_colour(q.bl, 255, 0, 0) && is_colour(q.br, 0, 255, 0)
     && is_colour(q.tl, 0, 0, 255) && is_colour(q.tr, 255, 255, 255))
    return "VERTICAL FLIP";
  if(is_colour(q.tr, 255, 0, 0) && is_colour(q.tl, 0, 255, 0)
     && is_colour(q.br, 0, 0, 255) && is_colour(q.bl, 255, 255, 255))
    return "HORIZONTAL FLIP";
  if(is_colour(q.br, 255, 0, 0) && is_colour(q.bl, 0, 255, 0)
     && is_colour(q.tr, 0, 0, 255) && is_colour(q.tl, 255, 255, 255))
    return "180 ROTATION";
  if(is_colour(q.tl, 255, 0, 0) && is_colour(q.bl, 0, 255, 0)
     && is_colour(q.tr, 0, 0, 255) && is_colour(q.br, 255, 255, 255))
    return "TRANSPOSE";
  return "unrecognised";
}

void check_key(const ReadbackImage& img, const char* what)
{
  const auto q = sample_quadrants(img);
  INFO(what << ": " << describe(q) << " -> " << diagnose(q));
  CHECK(diagnose(q) == std::string{"identity"});
}

std::vector<QString> chain_of_depth(const char* source, int passthroughs)
{
  std::vector<QString> c{corpus(source)};
  for(int i = 0; i < passthroughs; ++i)
    c.push_back(corpus("isf-passthrough-plain.fs"));
  return c;
}
}

// -----------------------------------------------------------------------------
// The delivered picture is the right way up to begin with.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the quadrant key is delivered in the documented orientation",
    "[gfx][l3][isf][orientation][matrix]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, chain_of_depth("isf-orient-quadrants.fs", 0));
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  check_key(r.outputs[0], "source only");
}

// -----------------------------------------------------------------------------
// A verbatim passthrough must be an identity, at every depth. This is the
// "video > window" vs "video > passthrough > window" vs
// "video > passthrough > passthrough > window" case.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a passthrough chain does not move the picture",
    "[gfx][l3][isf][orientation][matrix]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  std::vector<IsfResult> shots;
  for(int depth : {0, 1, 2})
  {
    IsfResult r = render(backend, chain_of_depth("isf-orient-quadrants.fs", depth));
    if(r.skipped)
      SKIP(r.backend + ": " + r.skip_reason);
    INFO("depth=" << depth << " backend=" << r.backend);
    REQUIRE(r.error.empty());
    REQUIRE(r.outputs.size() == 1);
    REQUIRE(r.outputs[0].valid());
    shots.push_back(std::move(r));
  }

  // Absolute: each depth is independently right way up. Without this the three
  // could agree while all being flipped.
  for(int depth : {0, 1, 2})
    check_key(shots[depth].outputs[0], depth == 0 ? "depth 0" : depth == 1 ? "depth 1"
                                                                          : "depth 2");

  // Relative: adding a passthrough changed nothing at all.
  for(int depth : {1, 2})
  {
    const int d = max_channel_diff(shots[0].outputs[0], shots[depth].outputs[0]);
    INFO("depth 0 vs depth " << depth << ": max channel diff " << d);
    CHECK(d <= kQuadTol);
  }
}

// -----------------------------------------------------------------------------
// Every backend must deliver the same picture through the same chain.
// -----------------------------------------------------------------------------
TEST_CASE(
    "every backend agrees on the quadrant key through a passthrough",
    "[gfx][l3][isf][orientation][matrix]")
{
  const int depth = GENERATE(0, 1, 2);
  CAPTURE(depth);

  const auto shots = render_all(chain_of_depth("isf-orient-quadrants.fs", depth));

  const ReadbackImage* reference = nullptr;
  std::string referenceName;
  for(const auto& s : shots)
  {
    if(s.result.skipped || !s.result.error.empty() || s.result.outputs.empty())
      continue;
    if(!s.result.outputs[0].valid())
      continue;

    INFO("backend=" << s.result.backend << " depth=" << depth);
    check_key(s.result.outputs[0], s.result.backend.c_str());

    if(!reference)
    {
      reference = &s.result.outputs[0];
      referenceName = s.result.backend;
      continue;
    }
    const int d = max_channel_diff(*reference, s.result.outputs[0]);
    INFO(referenceName << " vs " << s.result.backend << ": max channel diff " << d);
    CHECK(d <= kQuadTol);
  }
  if(!reference)
    SKIP("no backend produced a picture");
}

// -----------------------------------------------------------------------------
// The vertical ramp must survive the same chain: a flip that happens to keep
// the quadrant key plausible still shows up against a closed-form ramp.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the vertical ramp survives a passthrough chain",
    "[gfx][l3][isf][orientation][matrix]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const int depth = GENERATE(0, 1, 2);
  CAPTURE(backend_name(backend), depth);

  const IsfResult r = render(backend, chain_of_depth("isf-gradient-y.fs", depth));
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " depth=" << depth);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  const RampFit f = fit_vertical_ramp(img);
  INFO("worst row " << f.worstRow << ": green=" << f.got << " expected=" << f.expected);
  CHECK(f.worst <= kRampTol);
}

// -----------------------------------------------------------------------------
// The horizontal axis must be equally stable: it is the axis every existing
// cross-node assertion falls back on, so a regression there would go unseen.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the horizontal ramp survives a passthrough chain",
    "[gfx][l3][isf][orientation][matrix]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const int depth = GENERATE(0, 1, 2);
  CAPTURE(backend_name(backend), depth);

  const IsfResult r = render(backend, chain_of_depth("isf-gradient-x.fs", depth));
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " depth=" << depth);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  int worst = 0, worstCol = -1, got = 0, expected = 0;
  for(int x = 0; x < img.width; ++x)
  {
    const double t = (double(x) + 0.5) / double(img.width);
    const int want = int(std::lround(255.0 * t));
    for(int y : {1, img.height / 2, img.height - 2})
    {
      const int have = img.at(x, y)[0];
      if(std::abs(have - want) > worst)
      {
        worst = std::abs(have - want);
        worstCol = x;
        got = have;
        expected = want;
      }
    }
  }
  INFO("worst column " << worstCol << ": red=" << got << " expected=" << expected);
  CHECK(worst <= kRampTol);
}
