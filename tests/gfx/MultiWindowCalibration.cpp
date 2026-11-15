// The per-output calibration stage of the multi-window output: UV lookup warp
// map, alpha (blend) and beta (black level) maps, colour gain / offset /
// gamma, blending in linear light, and the corner warp applied live. Read back
// from the presented swap chain of a real window, so registered GUI and
// SKIPs without a windowing system.
//
// Content is tests/gfx/corpus/isf-gradient-x.fs: red = x, green = blue = 0.2.

#include "MultiWindowTestCommon.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score/tools/Zip.hpp>

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <mutex>

using namespace score::test;
using namespace score::test::gfx;
using Catch::Approx;

using namespace score::test::gfx::multiwindow;

namespace
{
QByteArray toJson(const Gfx::OutputMapping& m)
{
  JSONReader r;
  r.readFrom(m);
  return r.toByteArray();
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

TEST_CASE("multi-window alpha map applies in linear light", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString alpha = dir.filePath("alpha.pfm");
  writeGrey(alpha, 32, 16, 0.5f);

  auto m = single();
  m.maps.alpha = alpha;

  Frame plain, linear;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, plain);
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

  // Blending.hpp: out = content * alpha + beta in linear light, whatever the
  // linear blend setting, which only moves the edge ramps.
  CHECK(std::abs(plain.at(0.5, 0.5)[1] - u8(0.2 * std::pow(0.5, 1. / 2.2))) <= 3);
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

  CHECK(std::abs(f.at(0.75, 0.5)[1] - u8(std::pow(std::pow(0.2, 2.2) + 0.25, 1. / 2.2))) <= 3);
  // Outside the warped quad the content is black, the black level is not.
  CHECK(std::abs(f.at(0.2, 0.5)[1] - u8(std::pow(0.25, 1. / 2.2))) <= 3);
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
    setMaps(rig, 0, {});
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

TEST_CASE("OutputMapping output-stage fields round-trip and old documents load", "[gfx][window][calibration]")
{
  run_in_app([&](const score::GUIApplicationContext&) {
    Gfx::OutputMapping m;
    m.maps.warp = "/tmp/a.xyz.pfm";
    m.maps.alpha = "/tmp/alpha.pfm";
    m.maps.beta = "/tmp/beta.pfm";
    m.maps.lut = "/x/y.cube";
    m.maps.response = "/tmp/p.response.json";
    m.maps.compensation = "/tmp/p.comp.json";
    m.maps.calibration = "/cal/room.json";
    m.maps.blendSpace = Gfx::BlendMapSpace::Signal;
    m.color.gain = {0.9f, 1.f, 1.1f};
    m.color.offset = {0.01f, 0.02f, 0.03f};
    m.color.gamma = 1.2f;
    m.color.linearBlend = true;
    m.quality.sampling = Gfx::OutputSampling::Mipmapped;
    m.quality.warpInterpolation = Gfx::WarpInterpolation::Bilinear;
    m.quality.dither = Gfx::OutputDither::Off;
    m.quality.maxAnisotropy = 4;

    auto same = [&](const Gfx::OutputMapping& back) {
      CHECK(back.maps.warp == m.maps.warp);
      CHECK(back.maps.alpha == m.maps.alpha);
      CHECK(back.maps.beta == m.maps.beta);
      CHECK(back.maps.lut == m.maps.lut);
      CHECK(back.maps.response == m.maps.response);
      CHECK(back.maps.compensation == m.maps.compensation);
      CHECK(back.maps.calibration == m.maps.calibration);
      CHECK(back.maps.blendSpace == Gfx::BlendMapSpace::Signal);
      CHECK(back.color.gain == m.color.gain);
      CHECK(back.color.offset == m.color.offset);
      CHECK(back.color.gamma == Approx(1.2f));
      CHECK(back.color.linearBlend);
      CHECK(back.quality.sampling == m.quality.sampling);
      CHECK(back.quality.warpInterpolation == m.quality.warpInterpolation);
      CHECK(back.quality.dither == m.quality.dither);
      CHECK(back.quality.maxAnisotropy == 4);
    };
    {
      INFO("JSON");
      same(fromJson<Gfx::OutputMapping>(toJson(m)));
    }
    {
      INFO("DataStream");
      same(DataStreamWriter::unmarshall<Gfx::OutputMapping>(DataStreamReader::marshall(m)));
    }

    const auto old = fromJson<Gfx::OutputMapping>(QByteArray(
        R"({"SourceRect":[0,0,1,1],"ScreenIndex":-1,"WindowPosition":[100,100],)"
        R"("WindowSize":[1280,720],"Fullscreen":false,"BlendLeft":[0,2.2],)"
        R"("BlendRight":[0,2.2],"BlendTop":[0,2.2],"BlendBottom":[0,2.2]})"));
    CHECK(old.maps.empty());
    CHECK(old.maps.blendSpace == Gfx::BlendMapSpace::Auto);
    CHECK(old.color.isIdentity());
    CHECK(old.quality.isDefault());

    const auto defaults = toJson(Gfx::OutputMapping{});
    CHECK(defaults.indexOf("Color") < 0);
    CHECK(defaults.indexOf("Quality") < 0);
    CHECK(defaults.indexOf("Calibration") < 0);
    CHECK(defaults.indexOf("BlendMapSpace") < 0);
    auto linear = Gfx::OutputMapping{};
    linear.maps.blendSpace = Gfx::BlendMapSpace::Linear;
    CHECK(fromJson<Gfx::OutputMapping>(toJson(linear)).maps.blendSpace
          == Gfx::BlendMapSpace::Linear);
  });
}

namespace
{
//! u' = 1 - x, v' = y over W x H texels, valid on the left three quarters.
void writeFlipWarp(const QString& base, int W, int H)
{
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
  REQUIRE(score::gfx::writePFM(base + ".xyz.pfm", uv));
  REQUIRE(score::gfx::writePFM(base + ".valid.pfm", valid));
}

//! A valid warp sampling the source rectangle [u0;u1] x [v0;v1] over W x H texels.
void writeRangeWarp(const QString& base, int W, int H, float u0, float u1, float v0, float v1)
{
  score::gfx::FloatImage uv;
  uv.width = W;
  uv.height = H;
  uv.channels = 3;
  uv.data.resize(std::size_t(W) * H * 3);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      float* p = uv.row(y) + x * 3;
      p[0] = u0 + (u1 - u0) * (x + 0.5f) / W;
      p[1] = v0 + (v1 - v0) * (y + 0.5f) / H;
      p[2] = 0.f;
    }
  REQUIRE(score::gfx::writePFM(base + ".xyz.pfm", uv));
}

void writeFile(const QString& path, const QByteArray& bytes)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(bytes);
}

//! A one-region MPCDI archive of the given profile holding a 4x2 warp.
QString writeMpcdi(const QString& dir, const QString& profile)
{
  score::gfx::FloatImage warp;
  warp.width = 4;
  warp.height = 2;
  warp.channels = 3;
  warp.data.assign(4 * 2 * 3, 0.5f);
  writeFile(dir + "/warp.pfm", score::gfx::encodePFM(warp));
  writeFile(
      dir + "/mpcdi.xml",
      QStringLiteral(
          R"(<?xml version="1.0" encoding="utf-8"?>
<MPCDI profile="%1" level="1" version="2.0">
  <display>
    <buffer id="buf" xResolution="4" yResolution="2">
      <region id="proj1" x="0" y="0" xsize="1" ysize="1" xResolution="4" yResolution="2"/>
    </buffer>
  </display>
  <files>
    <fileset region="proj1">
      <geometryWarpFile><path>warp.pfm</path><interpolation>linear</interpolation></geometryWarpFile>
    </fileset>
  </files>
</MPCDI>)")
          .arg(profile)
          .toUtf8());
  const QString archive = dir + "/" + profile + ".mpcdi";
  QString err;
  REQUIRE(score::writeZipArchive(
      archive, {{dir + "/mpcdi.xml", "mpcdi.xml"}, {dir + "/warp.pfm", "warp.pfm"}}, 0,
      err));
  return archive;
}
}

TEST_CASE("multi-window warp map indexes the output pixel, like the alpha map", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeFlipWarp(dir.filePath("flip"), 64, 32);

  // The mirror then undoes the flip: the content comes back in place, and the
  // invalid quarter stays where the map has it, on the right of the screen.
  auto m = single();
  m.maps.warp = dir.filePath("flip.xyz.pfm");
  m.mirrorX = true;
  Frame f;
  bool ok{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) { ok = grab(rig, f); });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  CHECK(std::abs(f.at(0.1, 0.5)[0] - u8(0.1)) <= 8);
  CHECK(std::abs(f.at(0.6, 0.5)[0] - u8(0.6)) <= 8);
  CHECK(f.at(0.9, 0.5)[1] <= 2);
}

TEST_CASE("multi-window Auto sampling uses mips only when the warp minifies the canvas", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeRangeWarp(dir.filePath("whole"), 64, 32, 0.f, 1.f, 0.f, 1.f);
  writeRangeWarp(dir.filePath("detail"), 64, 32, 0.45f, 0.5f, 0.45f, 0.5f);

  int whole = -1, detail = -1;
  auto m = single();
  m.maps.warp = dir.filePath("whole.xyz.pfm");
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    Frame f;
    grab(rig, f);
    whole = rig.node->effectiveQuality(rig.node->windowOutputs()[0]).sampling;
    auto maps = m.maps;
    maps.warp = dir.filePath("detail.xyz.pfm");
    setMaps(rig, 0, maps);
    grab(rig, f);
    detail = rig.node->effectiveQuality(rig.node->windowOutputs()[0]).sampling;
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  CHECK(whole == 2);
  CHECK(detail == 0);
}

TEST_CASE("multi-window map of another size swaps in live", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeGrey(dir.filePath("half.pfm"), 4, 4, 0.5f);
  writeGrey(dir.filePath("quarter.pfm"), 8, 8, 0.25f);

  auto m = single();
  m.maps.alpha = dir.filePath("half.pfm");
  Frame half, quarter;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, half);
    auto maps = m.maps;
    maps.alpha = dir.filePath("quarter.pfm");
    setMaps(rig, 0, maps);
    ok2 = grab(rig, quarter);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  const double r = (std::floor(0.75 * 128) + 0.5) / 128.;
  CHECK(std::abs(half.at(0.75, 0.5)[0] - u8(std::pow(0.5, 1. / 2.2) * r)) <= 3);
  CHECK(std::abs(quarter.at(0.75, 0.5)[0] - u8(std::pow(0.25, 1. / 2.2) * r)) <= 3);
}

TEST_CASE("multi-window map loading: reloads, invalid texels, size limit, 3D MPCDI", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString alpha = dir.filePath("a.pfm");
  writeGrey(alpha, 4, 4, 0.5f);

  score::gfx::FloatImage nanWarp;
  nanWarp.width = 4;
  nanWarp.height = 2;
  nanWarp.channels = 3;
  nanWarp.data.assign(4 * 2 * 3, 0.5f);
  nanWarp.row(0)[3 * 1] = std::numeric_limits<float>::quiet_NaN();
  REQUIRE(score::gfx::writePFM(dir.filePath("nan.pfm"), nanWarp));

  const QString mpcdi2d = writeMpcdi(dir.path(), "2d");
  const QString mpcdi3d = writeMpcdi(dir.path(), "3d");

  int kept{}, cleared{-1}, tooWide{-1}, limit{};
  bool nanZeroed{}, othersValid{}, flat{}, world{true};
  auto o = runRig(api, single(), [&](MultiWindowRig& rig, Outcome&) {
    const auto& wo = rig.node->windowOutputs()[0];
    Gfx::OutputMaps maps;
    maps.alpha = alpha;
    setMaps(rig, 0, maps);
    writeFile(alpha, "not a float map");
    setMaps(rig, 0, maps);
    kept = wo.alphaMap ? wo.alphaMap->width : 0;
    maps.alpha = dir.filePath("missing.pfm");
    setMaps(rig, 0, maps);
    cleared = wo.alphaMap ? wo.alphaMap->width : 0;

    limit = rig.node->renderState()->rhi->resourceLimit(QRhi::TextureSizeMax);
    writeGrey(dir.filePath("wide.pfm"), limit + 1, 1, 1.f);
    maps.alpha = dir.filePath("wide.pfm");
    setMaps(rig, 0, maps);
    tooWide = wo.alphaMap ? wo.alphaMap->width : 0;

    maps = {};
    maps.warp = dir.filePath("nan.pfm");
    setMaps(rig, 0, maps);
    if(wo.warpMap)
    {
      const float* t = wo.warpMap->data.data();
      nanZeroed = t[4] == 0.f && t[5] == 0.f && t[6] == 0.f && t[7] == 0.f;
      othersValid = true;
      for(int k = 0; k < 8; k++)
        if(k != 1 && !(t[k * 4 + 3] == 1.f && std::isfinite(t[k * 4])))
          othersValid = false;
    }

    maps.warp = mpcdi2d;
    setMaps(rig, 0, maps);
    flat = bool(wo.warpMap);
    maps.warp = mpcdi3d;
    setMaps(rig, 0, maps);
    world = bool(wo.warpMap);
    rig.render(2);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  CHECK(kept == 4);
  CHECK(cleared == 0);
  INFO("texture size limit " << limit);
  CHECK(tooWide == 0);
  CHECK(nanZeroed);
  CHECK(othersValid);
  CHECK(flat);
  CHECK_FALSE(world);
}

TEST_CASE("multi-window live settings write through and the canvas only upgrades", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QRhiTexture::Format before{}, during{}, after{};
  Gfx::OutputColor stored;
  Gfx::CornerWarp storedWarp;
  float storedBlend{};
  auto o = runRig(api, single(), [&](MultiWindowRig& rig, Outcome&) {
    before = rig.node->offscreenTarget().texture->format();
    Gfx::OutputColor c;
    c.gain = {1.01f, 1.f, 1.f};
    rig.node->setColor(0, c);
    rig.render(3);
    during = rig.node->offscreenTarget().texture->format();
    rig.node->setColor(0, Gfx::OutputColor{});
    rig.render(3);
    after = rig.node->offscreenTarget().texture->format();

    c.offset = {0.f, 0.1f, 0.f};
    rig.node->setColor(0, c);
    Gfx::CornerWarp w;
    w.topLeft = {0.1, 0.};
    rig.node->setCornerWarp(0, w);
    rig.node->setEdgeBlend(0, 2, 0.2f, 1.5f);
    stored = rig.node->mappings()[0].color;
    storedWarp = rig.node->mappings()[0].cornerWarp;
    storedBlend = rig.node->mappings()[0].blendTop.width;
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  CHECK(before == QRhiTexture::RGBA8);
  CHECK(during == QRhiTexture::RGBA16F);
  CHECK(after == QRhiTexture::RGBA16F);
  CHECK(stored.gain[0] == 1.01f);
  CHECK(stored.offset[1] == 0.1f);
  CHECK(storedWarp.topLeft.x() == Approx(0.1));
  CHECK(storedBlend == 0.2f);
}

TEST_CASE("multi-window edge blend alone keeps an 8-bit canvas and no dither", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto m = single();
  m.blendLeft = {0.2f, 2.2f};
  m.blendTop = {0.1f, 1.5f};
  QRhiTexture::Format fmt{};
  int dither{-1};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    Frame f;
    grab(rig, f);
    rig.node->setEdgeBlend(0, 1, 0.15f, 2.f);
    rig.render(3);
    fmt = rig.node->offscreenTarget().texture->format();
    dither = rig.node->effectiveQuality(rig.node->windowOutputs()[0]).ditherLevels;
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  CHECK(fmt == QRhiTexture::RGBA8);
  CHECK(dither == 0);
}

TEST_CASE("multi-window Auto mips follow the output's size, per axis", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  // 64x32 texels over a tenth of the 1280x720 canvas: 2 and 2.25 canvas texels
  // per texel, 1 and 1.125 per pixel on the 128x64 window.
  writeRangeWarp(dir.filePath("mid"), 64, 32, 0.45f, 0.55f, 0.45f, 0.55f);
  auto m = single();
  m.maps.warp = dir.filePath("mid.xyz.pfm");

  bool shown{true}, flat{}, back{true}, small{};
  QSize refused;
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    auto mips = [&] {
      return rig.node->offscreenTarget().texture->flags().testFlag(QRhiTexture::MipMapped);
    };
    auto settle = [&](bool want) {
      pump_until([&] {
        rig.render(1);
        return mips() == want;
      }, 3000);
      return mips();
    };
    auto resize = [&](QSize sz) {
      rig.node->windowOutputs()[0].window->resize(sz);
      const bool reached = pump_until([&] {
        rig.render(1);
        const auto* sc = rig.node->windowOutputs()[0].swapChain;
        return sc && sc->currentPixelSize() == sz
               && rig.node->windowOutputs()[0].window->size() == sz;
      }, 3000);
      if(!reached && refused.isEmpty())
        refused = sz;
    };
    shown = settle(false);
    resize({128, 16});
    flat = settle(true);
    resize({128, 64});
    back = settle(false);
    resize({32, 16});
    small = settle(true);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  if(!refused.isEmpty())
    SKIP(o.backend << ": the window manager did not resize the output to " << refused.width()
                   << "x" << refused.height());
  CHECK_FALSE(shown);
  CHECK(flat);
  CHECK_FALSE(back);
  CHECK(small);
}

namespace
{
//! An MPCDI archive whose region proj1 holds an 8-bit alpha of 128 with gammaEmbedded 2.2.
QString writeAlphaMpcdi(const QString& dir)
{
  QImage grey{8, 8, QImage::Format_Grayscale8};
  grey.fill(128);
  REQUIRE(grey.save(dir + "/mpcdi-alpha.png"));
  writeFile(
      dir + "/mpcdi.xml",
      QByteArray(R"(<?xml version="1.0" encoding="utf-8"?>
<MPCDI profile="2d" level="1" version="2.0">
  <display>
    <buffer id="buf" xResolution="8" yResolution="8">
      <region id="proj1" x="0" y="0" xsize="1" ysize="1" xResolution="8" yResolution="8"/>
    </buffer>
  </display>
  <files>
    <fileset region="proj1">
      <alphaMap><path>alpha.png</path><componentDepth>1</componentDepth><bitDepth>8</bitDepth><gammaEmbedded>2.2</gammaEmbedded></alphaMap>
    </fileset>
  </files>
</MPCDI>)"));
  const QString archive = dir + "/blend.mpcdi";
  QString err;
  REQUIRE(score::writeZipArchive(
      archive, {{dir + "/mpcdi.xml", "mpcdi.xml"}, {dir + "/mpcdi-alpha.png", "alpha.png"}},
      0, err));
  return archive + "#proj1";
}
}

TEST_CASE("multi-window blend map space: float maps and MPCDI alpha in linear light, images in signal space", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeGrey(dir.filePath("alpha.pfm"), 8, 8, 0.5f);
  {
    QImage grey{8, 8, QImage::Format_Grayscale8};
    grey.fill(128);
    REQUIRE(grey.save(dir.filePath("alpha.png")));
  }
  const QString mpcdi = writeAlphaMpcdi(dir.path());

  struct Source
  {
    QString path;
    //! The alpha the map holds once read.
    double value;
    bool autoLinear;
  };
  const Source sources[]{
      {dir.filePath("alpha.pfm"), 0.5, true},
      {dir.filePath("alpha.png"), 128. / 255., false},
      {mpcdi, std::pow(128. / 255., 2.2), true}};
  using S = Gfx::BlendMapSpace;
  const S spaces[]{S::Auto, S::Linear, S::Signal};

  int got[3][3]{};
  bool ok = true;
  auto o = runRig(api, single(), [&](MultiWindowRig& rig, Outcome&) {
    for(int k = 0; k < 3; k++)
      for(int s = 0; s < 3; s++)
      {
        Gfx::OutputMaps maps;
        maps.alpha = sources[k].path;
        maps.blendSpace = spaces[s];
        setMaps(rig, 0, maps);
        Frame f;
        ok = grab(rig, f) && ok;
        got[k][s] = f.at(0.5, 0.5)[1];
      }
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  for(int k = 0; k < 3; k++)
    for(int s = 0; s < 3; s++)
    {
      const bool linear = spaces[s] == S::Linear || (spaces[s] == S::Auto && sources[k].autoLinear);
      const double a = sources[k].value;
      const int want = linear ? u8(0.2 * std::pow(a, 1. / 2.2)) : u8(0.2 * a);
      INFO("map " << sources[k].path.toStdString() << ", space " << s << ": " << got[k][s]
                  << ", expected " << want);
      CHECK(std::abs(got[k][s] - want) <= 3);
    }
}

namespace
{
struct WarningLog
{
  static inline std::mutex mutex;
  static inline QStringList lines;
  QtMessageHandler previous{};

  WarningLog()
  {
    std::lock_guard l{mutex};
    lines.clear();
    previous = qInstallMessageHandler(
        [](QtMsgType t, const QMessageLogContext&, const QString& msg) {
      if(t == QtWarningMsg)
      {
        std::lock_guard l{mutex};
        lines.push_back(msg);
      }
    });
  }
  ~WarningLog() { qInstallMessageHandler(previous); }

  int count(const QString& part) const
  {
    std::lock_guard l{mutex};
    int n = 0;
    for(const auto& line : lines)
      n += line.contains(part);
    return n;
  }
};
}

TEST_CASE("multi-window maps of another size than the output are reported once", "[gfx][window][multiwindow][calibration]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  writeGrey(dir.filePath("small.pfm"), 4, 4, 0.5f);
  writeGrey(dir.filePath("exact.pfm"), 128, 64, 0.5f);

  int stretched{-1}, exact{-1};
  auto o = runRig(api, single(), [&](MultiWindowRig& rig, Outcome&) {
    WarningLog log;
    Gfx::OutputMaps maps;
    maps.alpha = dir.filePath("small.pfm");
    setMaps(rig, 0, maps);
    setMaps(rig, 0, maps);
    rig.render(2);
    stretched = log.count(QStringLiteral("is 4x4 and is stretched to the 128x64 output"));
    maps.beta = dir.filePath("exact.pfm");
    setMaps(rig, 0, maps);
    exact = log.count(QStringLiteral("exact.pfm"));
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  CHECK(stretched == 1);
  CHECK(exact == 0);
}

TEST_CASE("a compensation offset of another size is reported", "[gfx][calibration][photometric]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("wall");
  score::gfx::FloatImage row;
  row.width = 4;
  row.height = 2;
  row.channels = 3;
  row.data.assign(4 * 2 * 3, 0.5f);
  for(int k = 0; k < 3; k++)
    REQUIRE(score::gfx::writePFM(base + QStringLiteral(".comp%1.pfm").arg(k), row));
  score::gfx::FloatImage off;
  off.width = 3;
  off.height = 3;
  off.channels = 3;
  off.data.assign(3 * 3 * 3, 0.1f);
  REQUIRE(score::gfx::writePFM(base + ".compoffset.pfm", off));

  WarningLog log;
  const auto m = score::gfx::loadCompensation(base + ".comp.json");
  REQUIRE(m.valid());
  CHECK(m.rows[0].data[3] == 0.f);
  CHECK(log.count(QStringLiteral("(3x3, 3 channels) does not match the 4x2 RGB rows")) == 1);
}
