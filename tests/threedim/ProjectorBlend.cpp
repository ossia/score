// Multi-projector blend and black-level maps on simulated walls: three
// projectors tiled on a flat wall, and four on a three-wall CAVE corner. The
// display seen by a simulated camera is the sum, at every surface point, of
// each projector's black + (content * alpha + beta) * white.

#include <Gfx/Calibration/Blending.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace blend = score::gfx::blend;
namespace procam = score::gfx::procam;
using blend::Vec2;
using blend::Vec3;

namespace
{
constexpr int PW = 240, PH = 150;

procam::Model lookAt(Vec3 pos, Vec3 target, double fx, double cx, double cy)
{
  procam::Model m;
  m.fx = m.fy = fx;
  m.cx = cx;
  m.cy = cy;
  const Vec3 z = (target - pos).normalized();
  Vec3 x = z.cross(Vec3(0., 1., 0.));
  if(x.norm() < 1e-6)
    x = Vec3(1., 0., 0.);
  x.normalize();
  const Vec3 y = z.cross(x);
  Eigen::Matrix3d wR;
  wR.col(0) = x;
  wR.col(1) = y;
  wR.col(2) = z;
  m.R = wR.transpose();
  m.t = -m.R * pos;
  return m;
}

using Surface = std::function<bool(const Vec3& o, const Vec3& d, Vec3& p)>;

bool flatWall(const Vec3& o, const Vec3& d, Vec3& p)
{
  if(d.z() >= 0.)
    return false;
  const double t = (-2. - o.z()) / d.z();
  p = o + t * d;
  return std::abs(p.x()) < 4. && p.y() > -1. && p.y() < 4.;
}

// Three walls of a 3 m CAVE: x = -1.5, x = 1.5, z = -1.5, floor y = 0, 3 m high.
bool cave(const Vec3& o, const Vec3& d, Vec3& p)
{
  double best = 1e9;
  if(d.x() < 0.)
    best = std::min(best, (-1.5 - o.x()) / d.x());
  if(d.x() > 0.)
    best = std::min(best, (1.5 - o.x()) / d.x());
  if(d.z() < 0.)
    best = std::min(best, (-1.5 - o.z()) / d.z());
  if(d.y() < 0.)
    best = std::min(best, -o.y() / d.y());
  if(best > 1e8)
    return false;
  p = o + best * d;
  return p.y() <= 3.;
}

blend::Projector makeProjector(const procam::Model& m, const Surface& s)
{
  blend::Projector p;
  p.width = PW;
  p.height = PH;
  p.model = m;
  p.hasModel = true;
  p.xyz.assign(PW * PH * 4, 0.f);
  const Eigen::Matrix3d Rt = m.R.transpose();
  const Vec3 o = m.position();
  for(int v = 0; v < PH; v++)
    for(int u = 0; u < PW; u++)
    {
      Vec3 X;
      if(!s(o, Rt * Vec3((u - m.cx) / m.fx, (v - m.cy) / m.fy, 1.), X))
        continue;
      float* q = p.xyz.data() + (v * PW + u) * 4;
      q[0] = float(X.x());
      q[1] = float(X.y());
      q[2] = float(X.z());
      q[3] = 1.f;
    }
  return p;
}

std::vector<blend::Projector> tiledWall()
{
  std::vector<blend::Projector> P;
  for(int i = 0; i < 3; i++)
  {
    const double x = -1.6 + 1.6 * i;
    P.push_back(makeProjector(
        lookAt(Vec3(x, 1.2, 0.), Vec3(x + 0.05 * (i - 1), 1.2, -2.), 200., 119.5, 74.5),
        flatWall));
  }
  return P;
}

std::vector<blend::Projector> caveProjectors()
{
  std::vector<blend::Projector> P;
  const Vec3 c(0., 2.9, 0.4);
  P.push_back(makeProjector(lookAt(c, Vec3(-1.5, 1.2, -0.2), 110., 119.5, 74.5), cave));
  P.push_back(makeProjector(lookAt(c, Vec3(-0.6, 1.2, -1.5), 110., 119.5, 74.5), cave));
  P.push_back(makeProjector(lookAt(c, Vec3(0.6, 1.2, -1.5), 110., 119.5, 74.5), cave));
  P.push_back(makeProjector(lookAt(c, Vec3(1.5, 1.2, -0.2), 110., 119.5, 74.5), cave));
  return P;
}

struct View
{
  int W, H;
  std::vector<float> white, black, coverage;
};

// A simulated camera: per pixel, each projector that lights the point with its
// nearest pixel adds black + (content * alpha + beta) * lum.
View render(
    const std::vector<blend::Projector>& P, const blend::Result* r, const Surface& s,
    const procam::Model& cam, int W, int H, float blackLevel,
    const std::vector<std::function<float(int, int, int)>>& lum = {})
{
  View out{W, H, std::vector<float>(W * H, 0.f), std::vector<float>(W * H, 0.f),
           std::vector<float>(W * H, 0.f)};
  const Eigen::Matrix3d Rt = cam.R.transpose();
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      Vec3 X;
      if(!s(cam.position(), Rt * Vec3((x - cam.cx) / cam.fx, (y - cam.cy) / cam.fy, 1.), X))
        continue;
      for(std::size_t j = 0; j < P.size(); j++)
      {
        Vec2 px;
        if(!P[j].model.project(X, px))
          continue;
        if(px.x() < -0.5 || px.y() < -0.5 || px.x() > PW - 0.5 || px.y() > PH - 0.5)
          continue;
        const int u = std::clamp(int(std::lround(px.x())), 0, PW - 1);
        const int v = std::clamp(int(std::lround(px.y())), 0, PH - 1);
        if(!P[j].valid(v * PW + u))
          continue;
        const float L = lum.empty() ? 1.f : lum[j](int(j), u, v);
        const float a = r ? r->alpha[j][v * PW + u] : 1.f;
        const float b = r ? r->beta[j][v * PW + u] : 0.f;
        out.white[y * W + x] += blackLevel * L + (a + b) * L;
        out.black[y * W + x] += blackLevel * L + b * L;
        out.coverage[y * W + x] += 1.f;
      }
    }
  return out;
}

// Camera pixels within r of an unlit one: the outer rim of the display, where
// the projector edges that bound it are point-sampled.
std::vector<float> interior(const View& v, int r)
{
  std::vector<float> out(v.coverage.size(), 0.f);
  for(int y = 0; y < v.H; y++)
    for(int x = 0; x < v.W; x++)
    {
      bool ok = v.coverage[y * v.W + x] > 0.f;
      for(int dy = -r; dy <= r && ok; dy++)
        for(int dx = -r; dx <= r && ok; dx++)
        {
          const int xx = x + dx, yy = y + dy;
          ok = xx >= 0 && yy >= 0 && xx < v.W && yy < v.H && v.coverage[yy * v.W + xx] > 0.f;
        }
      out[y * v.W + x] = ok ? 1.f : 0.f;
    }
  return out;
}

// The camera's optical blur: a box of radius r over the lit pixels.
View blurred(const View& v, int r)
{
  View out = v;
  for(int y = 0; y < v.H; y++)
    for(int x = 0; x < v.W; x++)
    {
      double w = 0., b = 0.;
      int n = 0;
      for(int dy = -r; dy <= r; dy++)
        for(int dx = -r; dx <= r; dx++)
        {
          const int xx = x + dx, yy = y + dy;
          if(xx < 0 || yy < 0 || xx >= v.W || yy >= v.H || v.coverage[yy * v.W + xx] <= 0.f)
            continue;
          w += v.white[yy * v.W + xx];
          b += v.black[yy * v.W + xx];
          n++;
        }
      if(n > 0)
      {
        out.white[y * v.W + x] = float(w / n);
        out.black[y * v.W + x] = float(b / n);
      }
    }
  return out;
}

// Largest relative step between lit neighbours (a seam shows as a jump).
double maxRelativeStep(const std::vector<float>& img, const std::vector<float>& cov, int W, int H)
{
  double m = 0.;
  for(int y = 1; y < H - 1; y++)
    for(int x = 1; x < W - 1; x++)
    {
      const int i = y * W + x;
      if(cov[i] <= 0.f || cov[i + 1] <= 0.f || cov[i + W] <= 0.f)
        continue;
      for(int j : {i + 1, i + W})
        m = std::max(m, std::abs(double(img[i]) - img[j]) / std::max(1e-6f, img[i]));
    }
  return m;
}

std::pair<double, double> range(const std::vector<float>& img, const std::vector<float>& cov)
{
  double lo = 1e9, hi = -1e9;
  for(std::size_t i = 0; i < img.size(); i++)
    if(cov[i] > 0.f)
    {
      lo = std::min(lo, double(img[i]));
      hi = std::max(hi, double(img[i]));
    }
  return {lo, hi};
}
}

TEST_CASE("distance to edge is the Euclidean distance to the lit border", "[blend]")
{
  blend::Projector p;
  p.width = 9;
  p.height = 7;
  p.xyz.assign(9 * 7 * 4, 0.f);
  for(int i = 0; i < 9 * 7; i++)
    p.xyz[i * 4 + 3] = 1.f;
  p.xyz[(3 * 9 + 4) * 4 + 3] = 0.f;
  const auto d = blend::distanceToEdge(p);
  CHECK(d[0] == Catch::Approx(0.));
  CHECK(d[3 * 9 + 4] == -1.f);
  CHECK(d[3 * 9 + 2] == Catch::Approx(1.));
  CHECK(d[1 * 9 + 2] == Catch::Approx(1.));
}

TEST_CASE("three tiled projectors: the alphas are a partition of unity", "[blend]")
{
  auto P = tiledWall();
  blend::Options opt;
  const auto r = blend::compute(P, opt);
  REQUIRE(r.alpha.size() == 3);
  CHECK(r.overlap[0] > 0.1f);
  CHECK(r.overlap[1] > 0.2f);

  const auto cam = lookAt(Vec3(0., 1.2, 1.), Vec3(0., 1.2, -2.), 120., 159.5, 59.5);
  const auto before = render(P, nullptr, flatWall, cam, 320, 120, 0.f);
  const auto after = render(P, &r, flatWall, cam, 320, 120, 0.f);
  double maxErr = 0.;
  int lit = 0, overlapped = 0;
  for(std::size_t i = 0; i < after.white.size(); i++)
  {
    if(after.coverage[i] <= 0.f)
      continue;
    lit++;
    overlapped += after.coverage[i] > 1.f;
    maxErr = std::max(maxErr, std::abs(after.white[i] - 1.));
  }
  INFO("lit " << lit << " overlapped " << overlapped << " max |sum - 1| " << maxErr);
  CHECK(overlapped > lit / 10);
  CHECK(maxErr < 0.02);
  const double seamBefore = maxRelativeStep(before.white, before.coverage, 320, 120);
  const double seamAfter = maxRelativeStep(after.white, after.coverage, 320, 120);
  INFO("seam before " << seamBefore << " after " << seamAfter);
  CHECK(seamBefore > 0.5);
  CHECK(seamAfter < 0.02);
}

TEST_CASE("the XYZ map inverted by search matches the pinhole reprojection", "[blend]")
{
  auto P = tiledWall();
  auto Q = P;
  for(auto& q : Q)
    q.hasModel = false;
  blend::Options opt;
  const auto a = blend::compute(P, opt);
  const auto b = blend::compute(Q, opt);
  double maxDiff = 0.;
  for(int i = 0; i < 3; i++)
    for(std::size_t k = 0; k < a.alpha[i].size(); k++)
      maxDiff = std::max(maxDiff, double(std::abs(a.alpha[i][k] - b.alpha[i][k])));
  INFO("max alpha difference " << maxDiff);
  CHECK(maxDiff < 0.03);
}

TEST_CASE("CAVE corner: four projectors on three walls and the floor", "[blend]")
{
  auto P = caveProjectors();
  for(auto& p : P)
    p.rampWidth = 40.f;
  blend::Options opt;
  opt.blackLevel = 0.02f;
  opt.blackCompensation = true;
  const auto r = blend::compute(P, opt);

  const auto cam = lookAt(Vec3(0., 1.6, 1.2), Vec3(0., 1.2, -1.5), 90., 159.5, 119.5);
  const auto before = render(P, nullptr, cave, cam, 320, 240, opt.blackLevel);
  const auto after = blurred(render(P, &r, cave, cam, 320, 240, opt.blackLevel), 1);

  const auto in = interior(after, 3);
  const auto [b0lo, b0hi] = range(before.black, in);
  std::vector<double> dBlack, dWhite;
  for(std::size_t i = 0; i < in.size(); i++)
    if(in[i] > 0.f)
    {
      dBlack.push_back(std::abs(after.black[i] / r.blackTarget - 1.));
      dWhite.push_back(std::abs(after.white[i] - 1. - 2. * opt.blackLevel));
    }
  auto pct = [](std::vector<double> v, double q) {
    std::sort(v.begin(), v.end());
    return v[std::size_t(q * (v.size() - 1))];
  };
  INFO("black before " << b0lo << ".." << b0hi << ", target " << r.blackTarget);
  INFO("black deviation p50 " << pct(dBlack, 0.5) << " p99 " << pct(dBlack, 0.99) << " max "
                              << pct(dBlack, 1.));
  INFO("white deviation p50 " << pct(dWhite, 0.5) << " p99 " << pct(dWhite, 0.99) << " max "
                              << pct(dWhite, 1.));
  CHECK(b0hi / b0lo > 3.9);
  CHECK(r.blackTarget == Catch::Approx(4 * opt.blackLevel));
  CHECK(pct(dBlack, 0.5) < 1e-3);
  CHECK(pct(dBlack, 0.99) < 0.05);
  CHECK(pct(dWhite, 0.99) < 0.05);
  CHECK(pct(dWhite, 1.) < 0.1);
  const double seam = maxRelativeStep(after.white, in, 320, 240);
  INFO("largest white step " << seam);
  CHECK(seam < 0.05);
}

TEST_CASE("luminance uniformity: vignetting and unequal projectors", "[blend]")
{
  auto P = tiledWall();
  const float gain[3] = {1.0f, 0.7f, 0.9f};
  std::vector<std::function<float(int, int, int)>> lum;
  for(int i = 0; i < 3; i++)
  {
    lum.push_back([&gain](int j, int u, int v) {
      const double dx = (u - 119.5) / 120., dy = (v - 74.5) / 75.;
      return float(gain[j] * (1. - 0.35 * (dx * dx + dy * dy)));
    });
    P[i].lum.resize(PW * PH * 3);
    for(int v = 0; v < PH; v++)
      for(int u = 0; u < PW; u++)
        for(int c = 0; c < 3; c++)
          P[i].lum[(v * PW + u) * 3 + c] = lum[i](i, u, v);
  }
  const auto cam = lookAt(Vec3(0., 1.2, 1.), Vec3(0., 1.2, -2.), 120., 159.5, 59.5);

  blend::Options geo;
  geo.photometric = false;
  const auto rg = blend::compute(P, geo);
  const auto vg = render(P, &rg, flatWall, cam, 320, 120, 0.f, lum);

  blend::Options uni;
  uni.luminanceUniformity = true;
  uni.hasViewer = true;
  uni.viewer = Vec3(0., 1.2, 1.);
  uni.maxLogSlope = 0.5;
  const auto ru = blend::compute(P, uni);
  const auto vu = render(P, &ru, flatWall, cam, 320, 120, 0.f, lum);

  blend::Options flat = uni;
  flat.maxLogSlope = 0.;
  const auto rf = blend::compute(P, flat);
  const auto vf = render(P, &rf, flatWall, cam, 320, 120, 0.f, lum);

  const double sg = maxRelativeStep(vg.white, vg.coverage, 320, 120);
  const double su = maxRelativeStep(vu.white, vu.coverage, 320, 120);
  const auto [glo, ghi] = range(vg.white, vg.coverage);
  const auto [ulo, uhi] = range(vu.white, vu.coverage);
  const auto [flo, fhi] = range(vf.white, vf.coverage);
  double mg = 0., mu = 0., mf = 0.;
  int n = 0;
  for(std::size_t i = 0; i < vu.white.size(); i++)
    if(vu.coverage[i] > 0.f)
    {
      mg += vg.white[i];
      mu += vu.white[i];
      mf += vf.white[i];
      n++;
    }
  INFO("geometric only: step " << sg << " range " << glo << ".." << ghi);
  INFO("constrained: step " << su << " range " << ulo << ".." << uhi);
  INFO("flat: range " << flo << ".." << fhi);
  INFO("mean white geometric " << mg / n << " constrained " << mu / n << " flat " << mf / n);
  CHECK(su < sg);
  CHECK(su < 0.03);
  CHECK(fhi / flo < 1.03);
  CHECK(mu > mf * 1.05);
  for(int i = 0; i < 3; i++)
    for(std::size_t k = 0; k < ru.alpha[i].size(); k++)
    {
      REQUIRE(ru.alpha[i][k] >= 0.f);
      REQUIRE(ru.alpha[i][k] <= 1.f);
    }
}

TEST_CASE("colour gains bring every projector to the common white point", "[blend]")
{
  auto P = tiledWall();
  const float tint[3][3] = {{1.0f, 1.0f, 1.0f}, {1.1f, 1.0f, 0.8f}, {0.9f, 1.0f, 1.2f}};
  for(int i = 0; i < 3; i++)
  {
    P[i].lum.resize(PW * PH * 3);
    for(int k = 0; k < PW * PH; k++)
      for(int c = 0; c < 3; c++)
        P[i].lum[k * 3 + c] = tint[i][c];
  }
  blend::Options opt;
  const auto r = blend::compute(P, opt);
  std::array<float, 3> ref{};
  for(int i = 0; i < 3; i++)
  {
    std::array<float, 3> w;
    for(int c = 0; c < 3; c++)
      w[c] = tint[i][c] * r.colorGain[i][c];
    const float y = 0.2126f * w[0] + 0.7152f * w[1] + 0.0722f * w[2];
    for(int c = 0; c < 3; c++)
      w[c] /= y;
    if(i == 0)
      ref = w;
    for(int c = 0; c < 3; c++)
    {
      CHECK(w[c] == Catch::Approx(ref[c]).epsilon(1e-3));
      CHECK(r.colorGain[i][c] <= 1.f);
    }
  }
}

TEST_CASE("per channel: tinted, vignetted projectors give one uniform white", "[blend][photometric]")
{
  auto P = tiledWall();
  const float tint[3][3] = {{1.0f, 1.0f, 1.0f}, {1.1f, 1.0f, 0.8f}, {0.85f, 1.0f, 1.2f}};
  const double vig[3][3] = {{0.35, 0.3, 0.25}, {0.2, 0.3, 0.45}, {0.4, 0.3, 0.2}};
  auto white = [&](int j, int c, int u, int v) {
    const double dx = (u - 119.5) / 120., dy = (v - 74.5) / 75.;
    return float(tint[j][c] * (1. - vig[j][c] * (dx * dx + dy * dy)));
  };
  for(int i = 0; i < 3; i++)
  {
    P[i].lum.resize(PW * PH * 3);
    for(int v = 0; v < PH; v++)
      for(int u = 0; u < PW; u++)
        for(int c = 0; c < 3; c++)
          P[i].lum[(v * PW + u) * 3 + c] = white(i, c, u, v);
  }
  blend::Options opt;
  opt.luminanceUniformity = true;
  opt.maxLogSlope = 0.;
  opt.perChannel = true;
  const auto r = blend::compute(P, opt);
  REQUIRE(r.channels == 3);
  blend::Options grey = opt;
  grey.perChannel = false;
  const auto rg = blend::compute(P, grey);
  REQUIRE(rg.channels == 1);

  const auto cam = lookAt(Vec3(0., 1.2, 1.), Vec3(0., 1.2, -2.), 120., 159.5, 59.5);
  std::array<View, 3> perCh, greyCh;
  for(int c = 0; c < 3; c++)
  {
    std::vector<std::function<float(int, int, int)>> lum;
    for(int j = 0; j < 3; j++)
      lum.push_back([&, c](int jj, int u, int v) { return white(jj, c, u, v); });
    blend::Result rc = r;
    for(int j = 0; j < 3; j++)
    {
      rc.alpha[j].resize(PW * PH);
      rc.beta[j].resize(PW * PH);
      for(int k = 0; k < PW * PH; k++)
      {
        rc.alpha[j][k] = r.alpha[j][k * 3 + c];
        rc.beta[j][k] = r.beta[j][k * 3 + c];
      }
    }
    perCh[c] = render(P, &rc, flatWall, cam, 320, 120, 0.f, lum);
    blend::Result gc = rg;
    for(int j = 0; j < 3; j++)
      for(int k = 0; k < PW * PH; k++)
        gc.alpha[j][k] *= rg.colorGain[j][c];
    greyCh[c] = render(P, &gc, flatWall, cam, 320, 120, 0.f, lum);
  }
  auto stats = [&](const std::array<View, 3>& V, double& lumRatio, double& chromaSpread) {
    double lo = 1e9, hi = 0., clo[2]{1e9, 1e9}, chi[2]{0., 0.};
    for(std::size_t i = 0; i < V[0].white.size(); i++)
    {
      if(V[0].coverage[i] <= 0.f)
        continue;
      const double R = V[0].white[i], G = V[1].white[i], B = V[2].white[i];
      const double Y = 0.2126 * R + 0.7152 * G + 0.0722 * B;
      lo = std::min(lo, Y);
      hi = std::max(hi, Y);
      const double cr[2]{R / G, B / G};
      for(int k = 0; k < 2; k++)
      {
        clo[k] = std::min(clo[k], cr[k]);
        chi[k] = std::max(chi[k], cr[k]);
      }
    }
    lumRatio = hi / lo;
    chromaSpread = std::max(chi[0] / clo[0], chi[1] / clo[1]) - 1.;
  };
  double lp, cp, lg, cg;
  stats(perCh, lp, cp);
  stats(greyCh, lg, cg);
  INFO("luminance max/min: per channel " << lp << ", grey + gains " << lg);
  INFO("chromaticity spread (R/G, B/G max/min - 1): per channel " << cp << ", grey + gains "
                                                                   << cg);
  CHECK(lp < 1.03);
  CHECK(cp < 0.03);
  CHECK(cg > 0.1);
  for(int j = 0; j < 3; j++)
    for(float a : r.alpha[j])
    {
      REQUIRE(a >= 0.f);
      REQUIRE(a <= 1.f);
    }
}
