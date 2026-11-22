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
#include <random>
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

TEST_CASE("Cube LUT files with non-finite values are refused", "[gfx][calibration][lut]")
{
  const QByteArray entries = "0 0 0\n0 0 1\n0 1 0\n0 1 1\n1 0 0\n1 0 1\n1 1 0\n";
  CHECK(score::gfx::parseCubeLut("LUT_3D_SIZE 2\n" + entries + "1 1 1\n").valid());
  QString err;
  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\n" + entries + "-inf 1 1\n", &err).valid());
  CHECK(!err.isEmpty());
  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\n" + entries + "1 1 1\nnan nan nan\n").valid());
  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\n" + entries + "1 1 1e40\n").valid());
  CHECK(!score::gfx::parseCubeLut("LUT_3D_SIZE 2\nDOMAIN_MAX inf 1 1\n" + entries + "1 1 1\n").valid());
  CHECK(!score::gfx::parseCubeLut("LUT_1D_SIZE 2\nLUT_1D_INPUT_RANGE 0 inf\n0 0 0\n1 1 1\n").valid());
  CHECK(!score::gfx::parseCubeLut("LUT_1D_SIZE 2\n0 0 0\n-nan 1 1\n").valid());
}

namespace
{
std::vector<float> voidAndCluster(int n)
{
  const int N = n * n;
  constexpr double sigma = 1.5;
  std::vector<double> kernel(N);
  for(int y = 0; y < n; y++)
    for(int x = 0; x < n; x++)
    {
      const int dx = std::min(x, n - x), dy = std::min(y, n - y);
      kernel[y * n + x] = std::exp(-(dx * dx + dy * dy) / (2. * sigma * sigma));
    }

  std::vector<char> bits(N, 0);
  std::vector<double> energy(N, 0.);
  auto splat = [&](int idx, double sign) {
    const int px = idx % n, py = idx / n;
    for(int y = 0; y < n; y++)
    {
      const int ky = ((y - py) % n + n) % n;
      for(int x = 0; x < n; x++)
      {
        const int kx = ((x - px) % n + n) % n;
        energy[y * n + x] += sign * kernel[ky * n + kx];
      }
    }
  };
  auto tightestCluster = [&] {
    int best = -1;
    for(int i = 0; i < N; i++)
      if(bits[i] && (best < 0 || energy[i] > energy[best]))
        best = i;
    return best;
  };
  auto largestVoid = [&] {
    int best = -1;
    for(int i = 0; i < N; i++)
      if(!bits[i] && (best < 0 || energy[i] < energy[best]))
        best = i;
    return best;
  };

  std::mt19937 rng{0x5eed};
  const int ones = std::max(1, N / 10);
  for(int placed = 0; placed < ones;)
  {
    const int i = int(rng() % N);
    if(!bits[i])
    {
      bits[i] = 1;
      splat(i, 1.);
      placed++;
    }
  }
  for(int guard = 0; guard < 4 * N; guard++)
  {
    const int c = tightestCluster();
    bits[c] = 0;
    splat(c, -1.);
    const int v = largestVoid();
    bits[v] = 1;
    splat(v, 1.);
    if(v == c)
      break;
  }

  std::vector<int> rank(N, 0);
  const auto prototype = bits;
  const auto prototypeEnergy = energy;
  for(int r = ones - 1; r >= 0; r--)
  {
    const int c = tightestCluster();
    bits[c] = 0;
    splat(c, -1.);
    rank[c] = r;
  }
  bits = prototype;
  energy = prototypeEnergy;
  for(int r = ones; r < N; r++)
  {
    const int v = largestVoid();
    bits[v] = 1;
    splat(v, 1.);
    rank[v] = r;
  }

  std::vector<float> out(N);
  for(int i = 0; i < N; i++)
    out[i] = (float(rank[i]) + 0.5f) / float(N);
  return out;
}

}

TEST_CASE("the blue-noise table is the void-and-cluster matrix", "[gfx][calibration][dither]")
{
  CHECK(score::gfx::blueNoise() == voidAndCluster(score::gfx::blueNoiseSize));

  const int size = GENERATE(score::gfx::blueNoiseSize, 32);
  INFO("size " << size);
  const int N = size * size;
  const auto n = size == score::gfx::blueNoiseSize ? score::gfx::blueNoise()
                                                   : voidAndCluster(size);
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
  writeGrey(alpha, 8, 8, std::pow(0.5f, 2.2f));

  // Green 0.2 x alpha 0.5^2.2 in linear light = 25.5 codes: without dither
  // every pixel rounds the same way, with it they average to 25.5.
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

TEST_CASE("multi-window Auto sampling keeps a corner warp linear", "[gfx][window][multiwindow][sampling]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto m = single();
  m.windowSize = {100, 50};
  m.cornerWarp.topLeft = {0.05, 0.05};
  m.cornerWarp.topRight = {0.95, 0.1};
  m.cornerWarp.bottomLeft = {0.1, 0.95};
  m.cornerWarp.bottomRight = {0.9, 0.9};

  Frame f;
  bool ok{}, mips{true};
  int sampling{-1};
  auto o = runRig(
      api, m,
      [&](MultiWindowRig& rig, Outcome&) {
    ok = grab(rig, f);
    mips = rig.node->offscreenTarget().texture->flags().testFlag(QRhiTexture::MipMapped);
    sampling = rig.node->effectiveQuality(rig.node->windowOutputs()[0]).sampling;
      },
      QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-checker-1px.fs"));
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  REQUIRE(ok);
  CHECK_FALSE(mips);
  CHECK(sampling == 0);

  // Linear taps of a one-pixel checker minified ten times: no averaging.
  double sum = 0., sum2 = 0.;
  int n = 0;
  for(int y = f.height / 3; y < 2 * f.height / 3; y++)
    for(int x = f.width / 3; x < 2 * f.width / 3; x++)
    {
      const double g = f.px(x, y)[1];
      sum += g;
      sum2 += g * g;
      n++;
    }
  const double mean = sum / n;
  const double sd = std::sqrt(std::max(0., sum2 / n - mean * mean));
  INFO("mean " << mean << " sd " << sd);
  CHECK(sd > 20.);
}
