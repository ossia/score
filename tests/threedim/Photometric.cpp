// Photometric calibration math on synthetic data: Debevec & Malik camera
// response recovery, the per channel projector response fit, and the
// radiometric compensation of a textured, coloured surface.

#include <Gfx/Calibration/Photometric.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

namespace ph = score::gfx::photometric;

namespace
{
// A camera response that is not a power law: toe and shoulder.
double cameraCurve(int c, double x)
{
  x = std::clamp(x, 0., 1.);
  const double a = 0.55 + 0.05 * c;
  return a * std::pow(x, 0.4) + (1. - a) * std::pow(x, 1.6);
}
}

TEST_CASE("Debevec & Malik: a non-power camera response is recovered", "[photometric]")
{
  std::mt19937 rng(7);
  std::uniform_real_distribution<double> logE(std::log(0.002), std::log(1.));
  std::normal_distribution<double> noise(0., 0.4);
  const std::vector<double> dt{1. / 16, 1. / 8, 1. / 4, 1. / 2, 1., 2., 4.};
  std::vector<double> lnDt;
  for(double t : dt)
    lnDt.push_back(std::log(t));
  const int N = 300, P = int(dt.size()), L = 256;
  for(int c = 0; c < 3; c++)
  {
    std::vector<int> Z(N * P);
    for(int i = 0; i < N; i++)
    {
      const double E = std::exp(logE(rng));
      for(int j = 0; j < P; j++)
        Z[i * P + j] = int(std::clamp(
            std::lround(255. * cameraCurve(c, E * dt[j]) + noise(rng)), 0l, 255l));
    }
    const auto g = ph::solveCameraResponse(Z, N, lnDt, L, 50.);
    double maxErr = 0.;
    for(int z = 12; z <= 243; z++)
    {
      auto inv = [&](double v) {
        double lo = 0., hi = 1.;
        for(int it = 0; it < 60; it++)
        {
          const double m = 0.5 * (lo + hi);
          (cameraCurve(c, m) < v ? lo : hi) = m;
        }
        return 0.5 * (lo + hi);
      };
      const double truth = inv(z / 255.) / inv(128. / 255.);
      const double est = std::exp(g[z] - g[128]);
      maxErr = std::max(maxErr, std::abs(est / truth - 1.));
    }
    INFO("channel " << c << ": max relative radiance error over Z 12..243 " << maxErr);
    CHECK(maxErr < 0.04);
  }
}

TEST_CASE("Projector response: per channel monotone fit and inverse", "[photometric]")
{
  const double gammas[3] = {2.0, 2.4, 2.2};
  std::mt19937 rng(3);
  std::normal_distribution<double> noise(0., 0.003);
  for(int c = 0; c < 3; c++)
  {
    auto f = [&](double x) { return 0.97 * std::pow(x, gammas[c]) + 0.03 * x; };
    std::vector<double> x, m;
    for(int k = 1; k <= 8; k++)
    {
      x.push_back(k / 8.);
      m.push_back(0.8 * f(k / 8.) + noise(rng));
    }
    const auto r = ph::fitChannelResponse(x, m);
    double fwd = 0., inv = 0.;
    for(int i = 0; i <= 100; i++)
    {
      const double t = i / 100.;
      fwd = std::max(fwd, std::abs(r(t) - f(t)));
      inv = std::max(inv, std::abs(f(r.inverse(t)) - t));
    }
    INFO("channel " << c << ": gamma " << r.gamma << " (true " << gammas[c]
                    << "), max |fit - f| " << fwd << ", max |f(inverse(t)) - t| " << inv);
    CHECK(std::abs(r.gamma - gammas[c]) < 0.15);
    CHECK(fwd < 0.015);
    CHECK(inv < 0.015);
    for(std::size_t k = 1; k < r.fitted.size(); k++)
      CHECK(r.fitted[k] > r.fitted[k - 1]);
  }
}

TEST_CASE("Radiometric compensation of a textured, coloured surface", "[photometric]")
{
  constexpr int W = 160, H = 100;
  // Projector primaries as the camera sees them on white: crosstalk.
  Eigen::Matrix3d Pm;
  Pm << 0.80, 0.10, 0.02, 0.12, 0.85, 0.08, 0.03, 0.10, 0.75;
  // A pink fabric with darker stripes and a blue-ish patch.
  auto rho = [](int x, int y) -> Eigen::Vector3d {
    Eigen::Vector3d r(0.95, 0.62, 0.70);
    if((x / 12) % 2)
      r *= 0.75;
    if(x > 100 && y > 50)
      r = Eigen::Vector3d(0.55, 0.65, 0.9);
    return r * (1. - 0.25 * std::pow((x - 80.) / 80., 2.));
  };
  const Eigen::Vector3d F(0.01, 0.01, 0.015);
  auto content = [](int x, int y) -> Eigen::Vector3d {
    return {0.15 + 0.7 * x / double(W - 1), 0.15 + 0.7 * y / double(H - 1),
            0.5 + 0.35 * std::sin(x * 0.07)};
  };

  std::vector<Eigen::Matrix3d> M(W * H);
  std::vector<double> scales;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const Eigen::Matrix3d V = rho(x, y).asDiagonal() * Pm;
      REQUIRE(ph::compensationMatrix(V, {1., 1., 1.}, M[y * W + x]));
      scales.push_back(ph::maxWhiteScale(M[y * W + x]));
    }
  std::sort(scales.begin(), scales.end());
  const double s = scales[std::size_t(0.05 * (scales.size() - 1))];

  // Before: the content projected as is; after: P = s M c, clipped. Each
  // result gets its best global gain against the target s c before the
  // comparison, so only the spatial and colour errors count.
  std::vector<Eigen::Vector3d> before(W * H), after(W * H), target(W * H);
  int clipped = 0;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const int i = y * W + x;
      const Eigen::Matrix3d V = rho(x, y).asDiagonal() * Pm;
      const Eigen::Vector3d c = content(x, y);
      target[i] = s * c;
      before[i] = V * c;
      Eigen::Vector3d p = s * M[i] * c;
      clipped += (p.array() < 0.).any() || (p.array() > 1.).any();
      p = p.cwiseMax(0.).cwiseMin(1.);
      after[i] = V * p;
    }
  auto score = [&](std::vector<Eigen::Vector3d> img, double& psnr, double& dE) {
    Eigen::Vector3d num = Eigen::Vector3d::Zero(), den = Eigen::Vector3d::Zero();
    for(int i = 0; i < W * H; i++)
    {
      num += img[i].cwiseProduct(target[i]);
      den += img[i].cwiseProduct(img[i]);
    }
    const Eigen::Vector3d gain = num.cwiseQuotient(den);
    std::vector<double> a, b;
    dE = 0.;
    for(int i = 0; i < W * H; i++)
    {
      const Eigen::Vector3d v = img[i].cwiseProduct(gain) / s;
      const Eigen::Vector3d t = target[i] / s;
      for(int c = 0; c < 3; c++)
      {
        a.push_back(std::pow(std::clamp(v[c], 0., 1.), 1. / 2.2));
        b.push_back(std::pow(std::clamp(t[c], 0., 1.), 1. / 2.2));
      }
      dE += ph::deltaE76({v[0], v[1], v[2]}, {t[0], t[1], t[2]});
    }
    psnr = ph::psnr(a, b);
    dE /= W * H;
  };
  double p0, e0, p1, e1;
  score(before, p0, e0);
  score(after, p1, e1);
  INFO("white scale s " << s << ", clipped pixels " << 100. * clipped / (W * H) << " %");
  INFO("uncompensated: PSNR " << p0 << " dB, mean dE76 " << e0);
  INFO("compensated: PSNR " << p1 << " dB, mean dE76 " << e1);
  CHECK(p1 > p0 + 10.);
  CHECK(e1 < 2.);
  CHECK(e0 > 5.);
}

TEST_CASE("Colour mixing: grid model, inverse and gamut table", "[photometric]")
{
  // Two projectors that are not additive: one dims mixtures (a picture mode
  // that boosts the primaries), one adds a white segment (DLP).
  auto dims = [](const std::array<double, 3>& l) {
    const double hi = std::max({l[0], l[1], l[2]});
    const double k = hi > 0. ? 1. - 0.4 * std::min({l[0], l[1], l[2]}) / hi : 1.;
    return std::array<double, 3>{k * l[0], k * l[1], k * l[2]};
  };
  auto dlp = [](const std::array<double, 3>& l) {
    const double m = 0.5 * std::min({l[0], l[1], l[2]});
    return std::array<double, 3>{l[0] + m, l[1] + m, l[2] + m};
  };
  for(int which = 0; which < 2; which++)
  {
    INFO((which ? "white segment" : "dimmed mixtures"));
    auto truth = [&](const std::array<double, 3>& l) { return which ? dlp(l) : dims(l); };
    constexpr int N = 5;
    ph::ColourMixing m;
    for(int c = 0; c < 3; c++)
      for(int k = 0; k < N; k++)
        m.axis[c].push_back(std::pow(k / double(N - 1), 2.2));
    for(int b = 0; b < N; b++)
      for(int g = 0; g < N; g++)
        for(int r = 0; r < N; r++)
          m.shown.push_back(truth({m.axis[0][r], m.axis[1][g], m.axis[2][b]}));
    REQUIRE(m.valid());

    std::mt19937 rng{11};
    std::uniform_real_distribution<double> u{0.05, 1.};
    double fwdErr = 0., invErr = 0.;
    for(int i = 0; i < 300; i++)
    {
      const std::array<double, 3> l{u(rng), u(rng), u(rng)};
      const auto t = truth(l);
      const auto f = m.forward(l);
      std::array<double, 3> back{};
      REQUIRE(m.solve(f, back));
      for(int c = 0; c < 3; c++)
      {
        fwdErr = std::max(fwdErr, std::abs(f[c] - t[c]));
        invErr = std::max(invErr, std::abs(back[c] - l[c]));
      }
    }
    CHECK(fwdErr < 0.08);
    CHECK(invErr < 1e-6);

    const auto inv = ph::MixingInverse::build(m);
    REQUIRE(inv.valid());
    double tableErr = 0.;
    for(int i = 0; i < 300; i++)
    {
      const std::array<double, 3> l{u(rng), u(rng), u(rng)};
      const auto e = m.forward(l);
      const auto got = m.forward(inv(e));
      for(int c = 0; c < 3; c++)
        tableErr = std::max(
            tableErr, std::abs(got[c] - e[c]) / std::max({e[0], e[1], e[2], 0.05}));
    }
    CHECK(tableErr < 0.04);

    // Out of reach: the table gives a darker colour of the same hue.
    const std::array<double, 3> far{1.4, 0.7, 0.35};
    const auto shown = m.forward(inv(far));
    CHECK(shown[0] < far[0]);
    CHECK(shown[1] / shown[0] == Catch::Approx(0.5).margin(0.03));
    CHECK(shown[2] / shown[0] == Catch::Approx(0.25).margin(0.03));

    const Eigen::Matrix3d M = Eigen::Matrix3d::Identity();
    const double s = ph::maxWhiteScale(M, m, inv);
    CHECK(s <= (which ? 1.5 : 0.6) * 1.001);
    CHECK(s >= (which ? 1.5 : 0.6) * 0.95);
  }
}
