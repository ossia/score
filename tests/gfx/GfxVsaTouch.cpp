// The vertexshaderart `touch` input, as VSA art reads it.
//
// On vertexshaderart.com (greggman/vertexshaderart, src/js/main.js) `touch` is
// a 32x240 RGBA float texture, NEAREST: column c is touch point c (column 0 the
// mouse or first finger), each texel (x, y, pressure, time) with x, y in clip
// space, y up. Every frame the history scrolls one row: the newest sample is
// row 0, at v = 0 of a raw texture() read, and v = 1 is 239 frames ago
// (CPUHistoryTexture: upload, then copyWithin one row down; HistoryTexture: the
// previous rows drawn one row up, the new one into row 0).
//
// In score that history is a texture source, TouchHistoryNode, which the
// window devices publish as the address `touch/history` (fed from their
// `touch/points`); an image input named `touch` is bound to it by default,
// with a 32x240 RGBA32F NEAREST target, in every shader format
// (ISFProcess.hpp). Nothing is implicit: a `touch` input bound or cabled to
// nothing reads nothing.
//
// Pinned here, on every backend:
// - the history read by a VSA through that target: texture / textureLod /
//   texelFetch read row 0 as the newest and row 5 as five frames ago, v = 1 as
//   the oldest (still at rest), column 1 the second finger, w a time that grows
//   with the rows, size 32x240; a v on the edge between two rows reads the
//   upper one, as OpenGL's nearest filtering does (VSA art often indexes rows
//   as n / 240);
// - the same history read by an ISF gives the same samples: v = 0 is the
//   newest in both formats. The ISF reads it with IMG_NORM_PIXEL_PREMULTIPLIED
//   / IMG_PIXEL_PREMULTIPLIED, the raw reads: IMG_NORM_PIXEL returns straight
//   colour, dividing by alpha, which holds the time here;
// - a VSA `touch` with nothing connected samples the empty placeholder, not a
//   history;
// - a texture cabled into `touch` instead reads with v = 0 at the bottom of the
//   image, as IMG_NORM_PIXEL does, on every backend.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/TouchHistoryNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>
#include <string>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
using Rgb = std::array<int, 3>;

// A touch sample as vsa-touch-probe.vs paints it.
constexpr Rgb shown(float x, float y, float pressure)
{
  return {int((x * 0.5f + 0.5f) * 255.f + 0.5f), int((y * 0.5f + 0.5f) * 255.f + 0.5f),
          int(pressure * 255.f + 0.5f)};
}

Rgb bandColor(const ReadbackImage& img, int band, int bands)
{
  const int x = int((band + 0.5) * img.width / bands);
  const auto p = img.at(x, img.height / 2);
  return {p[0], p[1], p[2]};
}

bool near(const Rgb& a, const Rgb& b, int tol = 3)
{
  for(int i = 0; i < 3; i++)
    if(std::abs(a[i] - b[i]) > tol)
      return false;
  return true;
}

std::string str(const Rgb& c)
{
  return "(" + std::to_string(c[0]) + ", " + std::to_string(c[1]) + ", "
         + std::to_string(c[2]) + ")";
}

// What the default binding of a `touch` input asks for (ISFProcess.hpp).
ossia::render_target_spec historyTarget()
{
  ossia::render_target_spec spec;
  spec.size = ossia::texture_size{32, 240};
  spec.format = ossia::texture_format::RGBA32F;
  spec.format_set = true;
  spec.mag_filter = spec.min_filter = ossia::texture_filter::NEAREST;
  spec.address_u = spec.address_v = spec.address_w
      = ossia::texture_address_mode::CLAMP_TO_EDGE;
  return spec;
}

// Ten frames at A, pressed, then five at B, released, with a second finger at
// C: rows 0-4 are B, row 5 is A, row 239 was never written.
template <typename Render>
void playGesture(score::gfx::PointerState& pointer, Render render)
{
  pointer.move(0, -0.5f, 0.5f);
  pointer.press(0, 1.f);
  render(10);
  pointer.move(0, 0.25f, -0.75f);
  pointer.press(0, 0.f);
  pointer.move(1, 0.75f, 0.f);
  pointer.press(1, 0.5f);
  render(5);
}

struct Shot
{
  bool skipped{};
  std::string skip_reason, backend, error;
  ReadbackImage img;
};
}

TEST_CASE("VSA touch: the history reads as on vertexshaderart", "[gfx][vsa][touch]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto pointers = std::make_shared<score::gfx::PointerState>();
    const int history = p.addNode(std::make_unique<score::gfx::TouchHistoryNode>(pointers));
    const int vsa = p.addVsa(corpus("vsa-touch-probe.vs"));
    if(vsa < 0 || history < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({72, 16});
    p.wire(p.nodeImageOut(history, 0), p.imageIn(vsa, 0));
    p.wire(p.imageOut(vsa, 0), p.sinkInput(sink));
    setRenderTargetSpec(*p.isf(vsa), first_image_input(*p.isf(vsa)), historyTarget());
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    playGesture(*pointers, [&](int n) { p.render(n); });
    out.img = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " error=" << out.error);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  const Rgb A = shown(-0.5f, 0.5f, 1.f);
  const Rgb B = shown(0.25f, -0.75f, 0.f);
  const Rgb C = shown(0.75f, 0.f, 0.5f);
  const Rgb rest = shown(0.f, 0.f, 0.f);
  const struct
  {
    const char* what;
    Rgb expected;
  } bands[] = {
      {"texture(v = 0): newest", B},
      {"texture(v = 0.01): row 2", B},
      {"texture(row 5)", A},
      {"texture(v = 1): oldest, never written", rest},
      {"textureLod(row 5)", A},
      {"texelFetch(row 5)", A},
      {"texture(column 1, v = 0): second finger", C},
      {"w: row 0 later than row 5, row 0 positive, 32x240", {255, 255, 255}},
      {"texture(v = 5 / 240): on the edge of rows 4 and 5, nearest takes row 5", A},
  };
  for(int b = 0; b < 9; b++)
  {
    const Rgb got = bandColor(out.img, b, 9);
    INFO("band " << b << " " << bands[b].what << ": got " << str(got) << ", expected "
                 << str(bands[b].expected));
    CHECK(near(got, bands[b].expected));
  }
}

TEST_CASE("ISF touch: the same history reads the same in an ISF", "[gfx][isf][touch]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto pointers = std::make_shared<score::gfx::PointerState>();
    const int history = p.addNode(std::make_unique<score::gfx::TouchHistoryNode>(pointers));
    const int isf = p.addIsf(corpus("isf-touch-probe.fs"));
    if(isf < 0 || history < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({64, 16});
    p.wire(p.nodeImageOut(history, 0), p.imageIn(isf, 0));
    p.wire(p.imageOut(isf, 0), p.sinkInput(sink));
    setRenderTargetSpec(*p.isf(isf), first_image_input(*p.isf(isf)), historyTarget());
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    playGesture(*pointers, [&](int n) { p.render(n); });
    out.img = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " error=" << out.error);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  const Rgb A = shown(-0.5f, 0.5f, 1.f);
  const Rgb B = shown(0.25f, -0.75f, 0.f);
  const Rgb C = shown(0.75f, 0.f, 0.5f);
  const struct
  {
    const char* what;
    Rgb expected;
  } bands[] = {
      {"IMG_NORM_PIXEL_PREMULTIPLIED(v = 0): newest", B},
      {"IMG_NORM_PIXEL_PREMULTIPLIED(row 5)", A},
      {"IMG_PIXEL_PREMULTIPLIED(row 5)", A},
      {"IMG_NORM_PIXEL_PREMULTIPLIED(column 1, v = 0): second finger", C},
  };
  for(int b = 0; b < 4; b++)
  {
    const Rgb got = bandColor(out.img, b, 4);
    INFO("band " << b << " " << bands[b].what << ": got " << str(got) << ", expected "
                 << str(bands[b].expected));
    CHECK(near(got, bands[b].expected));
  }
}

TEST_CASE("VSA touch: nothing connected is no history", "[gfx][vsa][touch]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int vsa = p.addVsa(corpus("vsa-touch-probe.vs"));
    if(vsa < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({72, 16});
    p.wire(p.imageOut(vsa, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    p.render(10);
    out.img = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " error=" << out.error);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  // Every read is the empty placeholder's (0, 0, 0, 0), and it is not 32x240.
  const Rgb zero = shown(0.f, 0.f, 0.f);
  for(int b : {0, 2, 5, 6})
  {
    const Rgb got = bandColor(out.img, b, 9);
    INFO("band " << b << ": got " << str(got) << ", expected " << str(zero));
    CHECK(near(got, zero));
  }
  const Rgb w = bandColor(out.img, 7, 9);
  INFO("band 7 (time / size): got " << str(w));
  CHECK(near(w, {0, 0, 0}));
}

TEST_CASE(
    "VSA touch: a cabled texture reads with v = 0 at the bottom on every backend",
    "[gfx][vsa][touch]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int up = p.addIsf(corpus("isf-orient-quadrants.fs"));
    const int vsa = p.addVsa(corpus("vsa-touch-raw.vs"));
    if(up < 0 || vsa < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({48, 16});
    p.wire(p.imageOut(up, 0), p.imageIn(vsa, 0));
    p.wire(p.imageOut(vsa, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    p.render(3);
    out.img = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " error=" << out.error);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  const Rgb blue{0, 0, 255}, red{255, 0, 0};
  const struct
  {
    const char* what;
    Rgb expected;
  } bands[] = {
      {"texture(0.25, 0.25): bottom-left", blue},
      {"texture(0.25, 0.75): top-left", red},
      {"texelFetch(0, 0): bottom-left", blue},
  };
  for(int b = 0; b < 3; b++)
  {
    const Rgb got = bandColor(out.img, b, 3);
    INFO("band " << b << " " << bands[b].what << ": got " << str(got) << ", expected "
                 << str(bands[b].expected));
    CHECK(near(got, bands[b].expected));
  }
}
