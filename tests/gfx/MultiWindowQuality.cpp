// Output-stage quality of the multi-window output: the .cube colour LUT,
// blue-noise triangular dither before the 8-bit swap chain, Catmull-Rom warp
// map interpolation and mip / anisotropic sampling of the canvas, plus the
// CPU-side pieces (Cube parser, void-and-cluster matrix, settings round trip).

#include "MultiWindowTestCommon.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/OutputQuality.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <set>

using namespace score::test;
using namespace score::test::gfx;

using namespace score::test::gfx::multiwindow;

namespace
{
//! A UV warp map of W x H texels: u' = f((x + 0.5) / W), v' = v.
void writeWarp(const QString& base, int W, int H, float (*f)(float))
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
      p[0] = f((x + 0.5f) / W);
      p[1] = (y + 0.5f) / H;
      p[2] = 0.f;
    }
  REQUIRE(score::gfx::writePFM(base + ".xyz.pfm", uv));
}
}

TEST_CASE("Cube LUT files parse as the Adobe 1.0 specification", "[gfx][calibration][lut]")
{
  QString err;
  auto lut = score::gfx::parseCubeLut(
      "# comment\nTITLE \"swap\"\nLUT_3D_SIZE 2\nDOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 1\n"
      "0 0 0\n0 0 1\n0 1 0\n0 1 1\n1 0 0\n1 0 1\n1 1 0\n1 1 1\n",
      &err);
  INFO(err.toStdString());
  REQUIRE(lut.valid());
  CHECK(lut.title == "swap");
  CHECK(lut.size == 2);
  // Red index fastest: entry 1 is (r = 1, g = 0, b = 0).
  CHECK(lut.rgb[3] == 0.f);
  CHECK(lut.rgb[5] == 1.f);

  auto oneD = score::gfx::parseCubeLut(
      "LUT_1D_SIZE 3\nLUT_1D_INPUT_RANGE 0 2\n0 0 0\n0.25 0.5 0.75\n1 1 1\n", &err);
  REQUIRE(oneD.valid());
  CHECK(oneD.size == 3);
  CHECK(oneD.domainMax[0] == 2.f);
  // The middle entry of every axis is the middle 1D entry, per channel.
  const std::size_t mid = (1 * 9 + 1 * 3 + 1) * 3;
  CHECK(std::abs(oneD.rgb[mid + 0] - 0.25f) < 1e-6f);
  CHECK(std::abs(oneD.rgb[mid + 1] - 0.5f) < 1e-6f);
  CHECK(std::abs(oneD.rgb[mid + 2] - 0.75f) < 1e-6f);

  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\n0 0 0\n", &err).valid());
  CHECK(!err.isEmpty());
  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\nDOMAIN_MIN 1 1 1\nDOMAIN_MAX 0 0 0\n").valid());
}

TEST_CASE("the void-and-cluster matrix is a blue-noise permutation", "[gfx][calibration][dither]")
{
  // 64 is the built-in table, 32 runs the generator.
  const int size = GENERATE(64, 32);
  INFO("size " << size);
  const int N = size * size;
  const auto& n = score::gfx::blueNoise(size);
  REQUIRE(int(n.size()) == N);
  std::set<int> ranks;
  for(float v : n)
    ranks.insert(int(std::floor(v * float(N))));
  CHECK(int(ranks.size()) == N);

  // Low-pass content: 8x8 block averages stay closer to 0.5 than white noise's
  // (standard deviation sqrt(1/12/64) = 0.036).
  const int blocks = size / 8;
  double sq = 0.;
  for(int by = 0; by < blocks; by++)
    for(int bx = 0; bx < blocks; bx++)
    {
      double s = 0.;
      for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
          s += n[(by * 8 + y) * size + bx * 8 + x];
      sq += (s / 64. - 0.5) * (s / 64. - 0.5);
    }
  const double rms = std::sqrt(sq / (blocks * blocks));
  INFO("8x8 block mean rms " << rms << ", white noise " << std::sqrt(1. / 12. / 64.));
  CHECK(rms < 0.5 * std::sqrt(1. / 12. / 64.));
}

TEST_CASE("multi-window colour LUT", "[gfx][window][multiwindow][calibration][lut]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.filePath("swap.cube");
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    // Swaps red and blue: entry (r, g, b) holds (b, g, r).
    QByteArray text = "LUT_3D_SIZE 2\n";
    for(int b = 0; b < 2; b++)
      for(int g = 0; g < 2; g++)
        for(int r = 0; r < 2; r++)
          text += QByteArray::number(b) + ' ' + QByteArray::number(g) + ' '
                  + QByteArray::number(r) + '\n';
    f.write(text);
  }

  auto m = single();
  m.maps.lut = path;
  Frame f;
  bool ok{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) { ok = grab(rig, f); });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  const auto p = f.at(0.75, 0.5);
  INFO(p[0] << "," << p[1] << "," << p[2]);
  CHECK(std::abs(p[0] - 51) <= 3);
  CHECK(std::abs(p[2] - int(0.75 * 255)) <= 5);
}

TEST_CASE("multi-window dither resolves levels between 8-bit codes", "[gfx][window][multiwindow][dither]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString alpha = dir.filePath("alpha.pfm");
  writeGrey(alpha, 8, 8, 0.5f);

  // Green 0.2 x alpha 0.5 = 25.5 codes: without dither every pixel rounds the
  // same way, with it they average to 25.5.
  auto stats = [](const Frame& f) {
    double sum = 0.;
    std::set<int> codes;
    int n = 0;
    for(int y = 8; y < f.height - 8; y++)
      for(int x = 8; x < f.width - 8; x++)
      {
        const int g = f.px(x, y)[1];
        sum += g;
        codes.insert(g);
        n++;
      }
    return std::make_pair(sum / n, codes);
  };

  auto m = single();
  m.maps.alpha = alpha;
  Frame on, off;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, on);
    auto q = rig.node->windowOutputs()[0].quality;
    q.dither = Gfx::OutputDither::Off;
    rig.node->setQuality(0, q);
    ok2 = grab(rig, off);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  const auto [meanOn, codesOn] = stats(on);
  const auto [meanOff, codesOff] = stats(off);
  INFO("dithered mean " << meanOn << " plain mean " << meanOff);
  CHECK(std::abs(meanOn - 25.5) < 0.2);
  CHECK(codesOn.size() >= 2);
  CHECK(codesOff.size() == 1);
}

TEST_CASE("multi-window warp map: Catmull-Rom reproduces a quadratic warp", "[gfx][window][multiwindow][warp]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("quad");
  writeWarp(base, 4, 4, [](float u) { return (2.f * u - 1.f) * (2.f * u - 1.f); });

  auto m = single();
  m.maps.warp = base + ".xyz.pfm";
  Frame cubic, linear;
  bool ok1{}, ok2{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, cubic);
    auto q = rig.node->windowOutputs()[0].quality;
    q.warpInterpolation = Gfx::WarpInterpolation::Bilinear;
    rig.node->setQuality(0, q);
    ok2 = grab(rig, linear);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  // u = 0.5: exact 0; bilinear between f(0.375) and f(0.625) gives 0.0625.
  const int c = cubic.px(cubic.width / 2, cubic.height / 2)[0];
  const int l = linear.px(linear.width / 2, linear.height / 2)[0];
  INFO("cubic " << c << " bilinear " << l);
  CHECK(c <= 3);
  CHECK(std::abs(l - 16) <= 4);
}

TEST_CASE("multi-window minification: mip and anisotropic sampling", "[gfx][window][multiwindow][sampling]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto spread = [](const Frame& f) {
    double sum = 0., sum2 = 0.;
    int n = 0;
    for(int y = 4; y < f.height - 4; y++)
      for(int x = 4; x < f.width - 4; x++)
      {
        const double g = f.px(x, y)[1];
        sum += g;
        sum2 += g * g;
        n++;
      }
    const double mean = sum / n;
    return std::make_pair(mean, std::sqrt(std::max(0., sum2 / n - mean * mean)));
  };

  auto m = single();
  m.windowSize = {100, 50};
  Frame plain, mip, aniso;
  bool ok1{}, ok2{}, ok3{};
  auto o = runRig(
      api, m,
      [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, plain);
    auto q = rig.node->windowOutputs()[0].quality;
    q.sampling = Gfx::OutputSampling::Mipmapped;
    rig.node->setQuality(0, q);
    ok2 = grab(rig, mip);
    q.sampling = Gfx::OutputSampling::Anisotropic;
    rig.node->setQuality(0, q);
    ok3 = grab(rig, aniso);
      },
      QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-checker-1px.fs"));
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  REQUIRE(ok3);
  const auto [pm, ps] = spread(plain);
  const auto [mm, ms] = spread(mip);
  const auto [am, as] = spread(aniso);
  INFO("linear " << pm << "+-" << ps << " mip " << mm << "+-" << ms << " aniso " << am
                 << "+-" << as);
  CHECK(ps > 20.);
  CHECK(std::abs(mm - 127.5) < 6.);
  CHECK(ms < 4.);
  CHECK(std::abs(am - 127.5) < 6.);
  CHECK(as < 4.);
}

TEST_CASE("multi-window equirectangular canvas orientation", "[gfx][window][multiwindow][canvas]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto m = single();
  m.windowSize = {128, 64};
  Frame plain, yawed, ignored;
  bool ok1{}, ok2{}, ok3{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    ok1 = grab(rig, plain);
    rig.node->setCanvasLayout({Gfx::CanvasProjection::Equirectangular, {0.f, 90.f, 0.f}});
    ok2 = grab(rig, yawed);
    rig.node->setCanvasLayout({Gfx::CanvasProjection::None, {0.f, 90.f, 0.f}});
    ok3 = grab(rig, ignored);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  REQUIRE(ok3);
  // Yaw +90 turns -Z towards -X: the output at longitude u shows u - 0.25.
  for(double fx : {0.125, 0.375, 0.5, 0.625, 0.875})
  {
    const double want = std::fmod(fx - 0.25 + 1.0, 1.0);
    INFO("fx " << fx);
    CHECK(std::abs(plain.at(fx, 0.5)[0] - u8(fx)) <= 3);
    CHECK(std::abs(yawed.at(fx, 0.5)[0] - u8(want)) <= 3);
    CHECK(std::abs(ignored.at(fx, 0.5)[0] - u8(fx)) <= 3);
  }
}

TEST_CASE("multi-window equirectangular canvas pitch looks up", "[gfx][window][multiwindow][canvas]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto m = single();
  m.windowSize = {128, 64};
  Frame up, down;
  bool ok1{}, ok2{};
  auto o = runRig(
      api, m,
      [&](MultiWindowRig& rig, Outcome&) {
    rig.node->setCanvasLayout({Gfx::CanvasProjection::Equirectangular, {45.f, 0.f, 0.f}});
    ok1 = grab(rig, up);
    rig.node->setCanvasLayout({Gfx::CanvasProjection::Equirectangular, {-45.f, 0.f, 0.f}});
    ok2 = grab(rig, down);
      },
      QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-y.fs"));
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  // Straight ahead (-Z, horizon) shows latitude +45 or -45: green 0.75 or 0.25.
  const int g1 = up.at(0.5, 0.5)[1];
  const int g2 = down.at(0.5, 0.5)[1];
  INFO("up " << g1 << " down " << g2);
  CHECK(std::abs(g1 - u8(0.75)) <= 4);
  CHECK(std::abs(g2 - u8(0.25)) <= 4);
}

TEST_CASE("multi-window points map: the canvas is looked up from the viewer position", "[gfx][window][multiwindow][canvas][points]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.filePath("wall.points.pfm");
  constexpr int W = 128, H = 64;
  {
    score::gfx::FloatImage img;
    img.width = W;
    img.height = H;
    img.channels = 3;
    img.data.resize(std::size_t(W) * H * 3);
    for(int y = 0; y < H; y++)
      for(int x = 0; x < W; x++)
      {
        float* p = img.row(y) + x * 3;
        p[0] = (x + 0.5f) / W * 4.f - 2.f;
        p[1] = 1.f - (y + 0.5f) / H * 2.f;
        p[2] = -2.f;
      }
    REQUIRE(score::gfx::writePFM(path, img));
  }
  auto m = single();
  m.windowSize = {W, H};
  m.maps.points = path;
  Frame atOrigin, moved, noProjection;
  bool ok1{}, ok2{}, ok3{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    rig.node->setCanvasLayout({Gfx::CanvasProjection::Equirectangular, {}, {0.f, 0.f, 0.f}});
    ok1 = grab(rig, atOrigin);
    rig.node->setCanvasLayout({Gfx::CanvasProjection::Equirectangular, {}, {1.f, 0.f, 0.f}});
    ok2 = grab(rig, moved);
    rig.node->setCanvasLayout({Gfx::CanvasProjection::None, {}, {1.f, 0.f, 0.f}});
    ok3 = grab(rig, noProjection);
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok1);
  REQUIRE(ok2);
  REQUIRE(ok3);
  auto expected = [](float X, float vx) {
    return float(std::atan2(X - vx, 2.f) / (2. * M_PI) + 0.5);
  };
  for(int x : {8, 40, 64, 90, 120})
  {
    const float X = (x + 0.5f) / W * 4.f - 2.f;
    INFO("x " << x);
    CHECK(std::abs(atOrigin.px(x, H / 2)[0] - u8(expected(X, 0.f))) <= 3);
    CHECK(std::abs(moved.px(x, H / 2)[0] - u8(expected(X, 1.f))) <= 3);
    CHECK(std::abs(noProjection.px(x, H / 2)[0] - u8((x + 0.5f) / W)) <= 3);
  }
}
