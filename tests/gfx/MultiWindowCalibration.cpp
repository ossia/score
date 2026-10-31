// The per-output calibration stage of the multi-window output: UV lookup warp
// map, alpha (blend) and beta (black level) maps, colour gain / offset /
// gamma, blending in linear light, and the corner warp applied live. Read back
// from the presented swap chain of a real window, so registered GUI and
// SKIPs without a windowing system.
//
// Content is tests/gfx/corpus/isf-gradient-x.fs: red = x, green = blue = 0.2.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <functional>

using namespace score::test;
using namespace score::test::gfx;
using Catch::Approx;

namespace
{
struct Frame
{
  int width{}, height{};
  bool bgra{};
  QByteArray data;

  //! RGB of the pixel at fractional position (fx, fy) of the frame, 0..255.
  std::array<int, 3> at(double fx, double fy) const
  {
    const int x = std::clamp(int(fx * width), 0, width - 1);
    const int y = std::clamp(int(fy * height), 0, height - 1);
    const auto* p = reinterpret_cast<const uint8_t*>(data.constData()) + (y * width + x) * 4;
    if(bgra)
      return {p[2], p[1], p[0]};
    return {p[0], p[1], p[2]};
  }
};

struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::string backend;
};

Gfx::OutputMapping single()
{
  Gfx::OutputMapping m;
  m.windowPosition = {20, 40};
  m.windowSize = {128, 64};
  return m;
}

bool grab(MultiWindowRig& rig, Frame& out)
{
  rig.render(2);
  rig.node->requestReadback(0);
  const auto& rb = rig.node->windowOutputs()[0].readback;
  for(int i = 0; i < 30 && rb->data.isEmpty(); i++)
    rig.render(1);
  if(rb->data.isEmpty())
    return false;
  out.width = rb->pixelSize.width();
  out.height = rb->pixelSize.height();
  out.bgra = rb->format == QRhiTexture::BGRA8;
  out.data = rb->data;
  return out.data.size() >= out.width * out.height * 4;
}

int u8(double v)
{
  return int(std::lround(std::clamp(v, 0., 1.) * 255.));
}

void writeGrey(const QString& path, int w, int h, float v)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = 1;
  img.data.assign(std::size_t(w) * h, v);
  REQUIRE(score::gfx::writePFM(path, img));
}

QByteArray toJson(const Gfx::OutputMapping& m)
{
  JSONReader r;
  r.readFrom(m);
  return r.toByteArray();
}

template <typename F>
Outcome runRig(score::gfx::GraphicsApi api, Gfx::OutputMapping mapping, F&& body)
{
  Outcome o;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, {mapping}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    body(rig, o);
  });
  return o;
}
}

TEST_CASE("multi-window colour gain, offset and gamma", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  auto m = single();
  m.color.gain = {1.f, 0.5f, 1.f};
  m.color.offset = {0.f, 0.f, 0.1f};

  Frame before, after;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, before);
    auto c = rig.node->windowOutputs()[0].color;
    c.gamma = 2.f;
    rig.node->setColor(0, c);
    ok2 = grab(rig, after);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);

  const auto c = before.at(0.5, 0.5);
  CHECK(std::abs(c[1] - u8(0.2 * 0.5)) <= 3);
  CHECK(std::abs(c[2] - u8(0.2 + 0.1)) <= 3);
  CHECK(std::abs(c[0] - u8(0.5)) <= 6);

  const auto g = after.at(0.5, 0.5);
  CHECK(std::abs(g[1] - u8(std::sqrt(0.1))) <= 3);
  CHECK(std::abs(g[2] - u8(std::sqrt(0.3))) <= 3);
}

TEST_CASE("multi-window alpha map, in gamma space and in linear light", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString alpha = dir.filePath("alpha.pfm");
  writeGrey(alpha, 32, 16, 0.5f);

  auto m = single();
  m.maps.alpha = alpha;

  Frame gammaSpace, linear;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, gammaSpace);
    auto c = rig.node->windowOutputs()[0].color;
    c.linearBlend = true;
    rig.node->setColor(0, c);
    ok2 = grab(rig, linear);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);

  CHECK(std::abs(gammaSpace.at(0.5, 0.5)[1] - u8(0.2 * 0.5)) <= 3);
  CHECK(
      std::abs(linear.at(0.5, 0.5)[1] - u8(0.2 * std::pow(0.5, 1. / 2.2))) <= 3);
}

TEST_CASE("multi-window beta map adds a black level", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString beta = dir.filePath("beta.pfm");
  writeGrey(beta, 8, 8, 0.25f);

  auto m = single();
  m.maps.beta = beta;
  m.cornerWarp.topLeft = {0.5, 0.0};
  m.cornerWarp.bottomLeft = {0.5, 1.0};

  Frame f;
  bool ok{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) { ok = grab(rig, f); });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);

  CHECK(std::abs(f.at(0.75, 0.5)[1] - u8(0.2 + 0.25)) <= 3);
  // Outside the warped quad the content is black, the black level is not.
  CHECK(std::abs(f.at(0.2, 0.5)[1] - u8(0.25)) <= 3);
}

TEST_CASE("multi-window UV warp map", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const int W = 64, H = 32;
  score::gfx::FloatImage uv;
  uv.width = W;
  uv.height = H;
  uv.channels = 3;
  score::gfx::FloatImage valid;
  valid.width = W;
  valid.height = H;
  valid.channels = 1;
  uv.data.resize(std::size_t(W) * H * 3);
  valid.data.resize(std::size_t(W) * H);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      float* p = uv.row(y) + x * 3;
      p[0] = 1.f - (x + 0.5f) / W;
      p[1] = (y + 0.5f) / H;
      p[2] = 0.f;
      valid.row(y)[x] = x < 3 * W / 4 ? 1.f : 0.f;
    }
  REQUIRE(score::gfx::writePFM(dir.filePath("flip.xyz.pfm"), uv));
  REQUIRE(score::gfx::writePFM(dir.filePath("flip.valid.pfm"), valid));

  auto m = single();
  m.maps.warp = dir.filePath("flip.xyz.pfm");

  Frame f, cleared;
  bool ok{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok = grab(rig, f);
    rig.node->setMaps(0, {});
    ok2 = grab(rig, cleared);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  REQUIRE(ok2);

  CHECK(std::abs(f.at(0.1, 0.5)[0] - u8(0.9)) <= 8);
  CHECK(std::abs(f.at(0.6, 0.5)[0] - u8(0.4)) <= 8);
  const auto masked = f.at(0.9, 0.5);
  CHECK(masked[0] <= 2);
  CHECK(masked[1] <= 2);

  CHECK(std::abs(cleared.at(0.1, 0.5)[0] - u8(0.1)) <= 8);
  CHECK(std::abs(cleared.at(0.9, 0.5)[0] - u8(0.9)) <= 8);
}

TEST_CASE("multi-window corner warp applies live", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Frame before, after;
  bool ok1{}, ok2{};
  auto o = runRig(api, single(), [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, before);
    Gfx::CornerWarp w;
    w.topLeft = {0.5, 0.0};
    w.bottomLeft = {0.5, 1.0};
    rig.node->setCornerWarp(0, w);
    ok2 = grab(rig, after);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);

  CHECK(std::abs(before.at(0.25, 0.5)[1] - u8(0.2)) <= 3);
  CHECK(after.at(0.25, 0.5)[1] <= 2);
  CHECK(std::abs(after.at(0.75, 0.5)[0] - u8(0.5)) <= 8);
}

TEST_CASE("OutputMapping calibration fields round-trip and old documents load", "[gfx][window][calibration]")
{
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    Gfx::OutputMapping m;
    m.maps.warp = "/tmp/a.xyz.pfm";
    m.maps.alpha = "/tmp/alpha.pfm";
    m.maps.beta = "/tmp/beta.pfm";
    m.color.gain = {0.9f, 1.f, 1.1f};
    m.color.offset = {0.01f, 0.02f, 0.03f};
    m.color.gamma = 1.2f;
    m.color.linearBlend = true;

    const auto json = toJson(m);
    const auto back = fromJson<Gfx::OutputMapping>(json);
    CHECK(back.maps.warp == m.maps.warp);
    CHECK(back.maps.alpha == m.maps.alpha);
    CHECK(back.maps.beta == m.maps.beta);
    CHECK(back.color.gain == m.color.gain);
    CHECK(back.color.offset == m.color.offset);
    CHECK(back.color.gamma == Approx(1.2f));
    CHECK(back.color.linearBlend);

    const auto fromDs
        = DataStreamWriter::unmarshall<Gfx::OutputMapping>(DataStreamReader::marshall(m));
    CHECK(fromDs.maps.warp == m.maps.warp);
    CHECK(fromDs.color.gain == m.color.gain);
    CHECK(fromDs.color.linearBlend);

    const auto old = fromJson<Gfx::OutputMapping>(QByteArray(
        R"({"SourceRect":[0,0,1,1],"ScreenIndex":-1,"WindowPosition":[100,100],)"
        R"("WindowSize":[1280,720],"Fullscreen":false,"BlendLeft":[0,2.2],)"
        R"("BlendRight":[0,2.2],"BlendTop":[0,2.2],"BlendBottom":[0,2.2]})"));
    CHECK(old.maps.empty());
    CHECK(old.color.isIdentity());

    CHECK(toJson(Gfx::OutputMapping{}).indexOf("Color") < 0);
  });
}
