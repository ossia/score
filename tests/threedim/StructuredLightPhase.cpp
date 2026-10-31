// Gray code + phase shift against Gray code alone, on a simulated bench where
// the Gray code is resolution-limited: a 1920x1080 projector with a projector
// gamma of 2.2, seen by a 640x360 camera (pixel area integration, Gaussian
// blur sigma 0.75 px, additive noise) on a two-wall corner. Every case
// compares the decode and the calibration with the ground truth.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/StructuredLightCalibration.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

using Threedim::StructuredLightCalibration;
namespace procam = score::gfx::procam;
namespace sl = Threedim::sl;

namespace
{
constexpr int PW = 1920, PH = 1080;
constexpr int CW = 640, CH = 360;

Eigen::Matrix3d yawPitch(double yaw, double pitch)
{
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  return (Eigen::AngleAxisd(yaw, Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(pitch, Eigen::Vector3d::UnitX()))
             .toRotationMatrix()
         * flip;
}

procam::Model projector()
{
  procam::Model m;
  m.fx = 2600.;
  m.fy = 2590.;
  m.cx = 980.;
  m.cy = 1010.;
  const Eigen::Matrix3d wR = yawPitch(0.80, -0.12);
  const Eigen::Vector3d pos(0.4, 1.6, 0.5);
  m.R = wR.transpose();
  m.t = -m.R * pos;
  return m;
}

bool hitCorner(const Eigen::Vector3d& o, const Eigen::Vector3d& d, Eigen::Vector3d& p)
{
  double best = 1e9;
  if(d.z() < 0.)
    best = std::min(best, (-1.5 - o.z()) / d.z());
  if(d.x() < 0.)
    best = std::min(best, (-1.5 - o.x()) / d.x());
  if(best > 1e8)
    return false;
  p = o + best * d;
  return true;
}

struct Sim
{
  procam::Model proj = projector();
  double fx = 520., fy = 520., cx = 319.5, cy = 179.5;
  Eigen::Matrix4d worldFromCamera = Eigen::Matrix4d::Identity();
  float projectorGamma = 2.2f;
  float noise = 0.005f;
  //! Depth noise sigma at 3 m, growing with the square of the depth.
  float depthNoise = 0.f;
  //! Only the central fraction of the projector image is lit (1 = all).
  float litFraction = 1.f;
  std::vector<float> noisyPositions;
  static constexpr int SS = 2;

  std::vector<Eigen::Vector3d> worldSub;
  std::vector<uint8_t> hasSub;
  std::vector<Eigen::Vector2d> projSub;
  std::vector<uint8_t> inProj;
  std::vector<Eigen::Vector2d> truth;
  std::vector<uint8_t> hasTruth;
  std::vector<float> image, positions, tmp;
  std::mt19937 rng{7};

  Sim()
  {
    worldFromCamera.topLeftCorner<3, 3>() = yawPitch(0.72, -0.18);
    worldFromCamera.topRightCorner<3, 1>() = Eigen::Vector3d(0.7, 1.7, 0.7);
    const Eigen::Matrix3d R = worldFromCamera.topLeftCorner<3, 3>();
    const Eigen::Vector3d o = worldFromCamera.topRightCorner<3, 1>();
    const int SW = CW * SS, SH = CH * SS;
    worldSub.resize(std::size_t(SW) * SH);
    hasSub.assign(worldSub.size(), 0);
    projSub.resize(worldSub.size());
    inProj.assign(worldSub.size(), 0);
    for(int y = 0; y < SH; y++)
      for(int x = 0; x < SW; x++)
      {
        const double px = (x + 0.5) / SS - 0.5, py = (y + 0.5) / SS - 0.5;
        const Eigen::Vector3d ray((px - cx) / fx, (py - cy) / fy, 1.);
        Eigen::Vector3d p;
        const std::size_t i = std::size_t(y) * SW + x;
        if(!hitCorner(o, R * ray, p))
          continue;
        worldSub[i] = p;
        hasSub[i] = 1;
        Eigen::Vector2d q;
        if(proj.project(p, q) && q.x() > -0.5 && q.y() > -0.5 && q.x() < PW - 0.5
           && q.y() < PH - 0.5)
        {
          projSub[i] = q;
          inProj[i] = 1;
        }
      }
    truth.resize(CW * CH);
    hasTruth.assign(CW * CH, 0);
    positions.assign(CW * CH * 4, 0.f);
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        const Eigen::Vector3d ray((x - cx) / fx, (y - cy) / fy, 1.);
        Eigen::Vector3d p;
        const int i = y * CW + x;
        if(!hitCorner(o, R * ray, p))
          continue;
        const Eigen::Vector3d pc = R.transpose() * (p - o);
        for(int c = 0; c < 3; c++)
          positions[i * 4 + c] = float(pc[c]);
        positions[i * 4 + 3] = 1.f;
        Eigen::Vector2d q;
        if(proj.project(p, q) && q.x() > 4 && q.y() > 4 && q.x() < PW - 5 && q.y() < PH - 5)
        {
          truth[i] = q;
          hasTruth[i] = 1;
        }
      }
    image.assign(CW * CH * 4, 0.f);
    tmp.assign(CW * CH, 0.f);
  }

  const float* positionsFrame()
  {
    if(depthNoise <= 0.f)
      return positions.data();
    noisyPositions = positions;
    std::normal_distribution<float> g(0.f, 1.f);
    for(int i = 0; i < CW * CH; i++)
    {
      float* p = &noisyPositions[i * 4];
      if(p[3] <= 0.f)
        continue;
      const float z = p[2];
      const float s = 1.f + depthNoise * (z / 3.f) * (z / 3.f) * g(rng) / z;
      p[0] *= s;
      p[1] *= s;
      p[2] *= s;
    }
    return noisyPositions.data();
  }

  void render(const uint8_t* pattern, int pw, int ph)
  {
    const int SW = CW * SS;
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        float acc = 0.f;
        for(int sy = 0; sy < SS; sy++)
          for(int sx = 0; sx < SS; sx++)
          {
            const std::size_t i = std::size_t(y * SS + sy) * SW + x * SS + sx;
            float l = hasSub[i] ? 0.03f : 0.f;
            if(pattern && inProj[i])
            {
              const int u = std::clamp(int(std::floor(projSub[i].x() + 0.5)), 0, pw - 1);
              const int v = std::clamp(int(std::floor(projSub[i].y() + 0.5)), 0, ph - 1);
              const float m = 0.5f * (1.f - litFraction);
              const bool lit = u >= m * pw && u < (1.f - m) * pw && v >= m * ph
                               && v < (1.f - m) * ph;
              const float code = pattern[(std::size_t(v) * pw + u) * 4] / 255.f;
              if(lit)
                l += 0.8f * std::pow(code, projectorGamma);
            }
            acc += l;
          }
        tmp[y * CW + x] = acc / (SS * SS);
      }
    // Separable Gaussian, sigma 0.75 px.
    const float kern[5] = {0.0152f, 0.2187f, 0.5322f, 0.2187f, 0.0152f};
    std::vector<float> h(CW * CH);
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        float a = 0.f;
        for(int d = -2; d <= 2; d++)
          a += kern[d + 2] * tmp[y * CW + std::clamp(x + d, 0, CW - 1)];
        h[y * CW + x] = a;
      }
    std::normal_distribution<float> n(0.f, std::max(noise, 1e-6f));
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        float a = 0.f;
        for(int d = -2; d <= 2; d++)
          a += kern[d + 2] * h[std::clamp(y + d, 0, CH - 1) * CW + x];
        a += noise > 0.f ? n(rng) : 0.f;
        float* p = &image[(y * CW + x) * 4];
        p[0] = p[1] = p[2] = a;
        p[3] = 1.f;
      }
  }
};

struct Config
{
  const char* name;
  bool phase{};
  int steps{4};
  int period{0};
  int minStripe{2};
  float compensation{2.2f};
  float ransac{0.f};
  int surface{StructuredLightCalibration::Planes};
  int kmode{StructuredLightCalibration::SolveIntrinsics};
  std::array<float, 4> lockedK{};
};

struct Metrics
{
  double decodedPct{}, phasePct{};
  double medianErr{}, p90Err{}, phaseMedianErr{}, phaseP90Err{};
  double periodJumps{};
  double fxErr{}, cxErr{}, cyErr{}, posErrMm{}, rotErrDeg{}, rms{}, xyzRmsMm{};
  int patterns{}, period{};
};

double percentile(std::vector<double> v, double q)
{
  if(v.empty())
    return 0.;
  const std::size_t k = std::min(v.size() - 1, std::size_t(q * (v.size() - 1)));
  std::nth_element(v.begin(), v.begin() + k, v.end());
  return v[k];
}

Metrics runConfig(const Config& c, Sim& sim)
{
  StructuredLightCalibration obj;
  obj.inputs.projector_size.value = {PW, PH};
  obj.inputs.world_from_camera.value = procam::toColumnMajor(sim.worldFromCamera);
  obj.inputs.settle.value = 100;
  obj.inputs.frames.value = 1;
  obj.inputs.min_stripe.value = c.minStripe;
  obj.inputs.phase_shift.value = c.phase;
  obj.inputs.phase_steps.value = c.steps;
  obj.inputs.phase_period.value = c.period;
  obj.inputs.projector_gamma.value = c.compensation;
  obj.inputs.surface.value = StructuredLightCalibration::Surface(c.surface);
  obj.inputs.max_planes.value = 2;
  obj.inputs.hole_radius.value = 8;
  obj.inputs.ransac.value = c.ransac;
  obj.inputs.intrinsics_mode.value = StructuredLightCalibration::IntrinsicsMode(c.kmode);
  obj.inputs.locked_intrinsics.value
      = {c.lockedK[0], c.lockedK[1], c.lockedK[2], c.lockedK[3]};

  double now = 1000.;
  obj.tick(now);
  obj.inputs.start.value = halp::impulse_type{};
  for(int i = 0; i < 100000 && !obj.outputs.done.value; i++)
  {
    auto& pat = obj.outputs.pattern.texture;
    if(obj.inputs.camera.request_readback)
      sim.render(pat.bytes, pat.width, pat.height);
    obj.inputs.camera.texture
        = {obj.inputs.camera.request_readback ? sim.image.data() : nullptr, CW, CH, true};
    obj.inputs.positions.texture = {
        obj.inputs.positions.request_readback ? const_cast<float*>(sim.positionsFrame())
                                              : nullptr,
        CW, CH, true};
    now += 120.;
    obj.tick(now);
    obj.inputs.start.value.reset();
    if(!obj.running() && !obj.outputs.done.value)
      break;
    if(obj.outputs.status.value == "Decoding")
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  INFO(obj.outputs.status.value);
  REQUIRE(obj.outputs.done.value);
  if(c.kmode != StructuredLightCalibration::SolveIntrinsics)
    CHECK(obj.outputs.status.value.find("intrinsics locked") != std::string::npos);
  const auto* r = obj.lastResult();
  REQUIRE(r);

  Metrics m;
  m.patterns = r->seq.count();
  m.period = r->seq.period;
  std::vector<double> err, perr;
  int decoded = 0, phase = 0, considered = 0, jumps = 0;
  for(int i = 0; i < CW * CH; i++)
  {
    if(!sim.hasTruth[i])
      continue;
    considered++;
    const auto& d = r->pixels[i];
    if(!d.valid)
      continue;
    decoded++;
    const double e = std::hypot(d.u - sim.truth[i].x(), d.v - sim.truth[i].y());
    err.push_back(e);
    if(d.phaseU && d.phaseV)
    {
      phase++;
      perr.push_back(e);
      if(c.phase && e > 0.5 * r->seq.period)
        jumps++;
    }
  }
  m.decodedPct = 100. * decoded / std::max(1, considered);
  m.phasePct = 100. * phase / std::max(1, decoded);
  m.medianErr = percentile(err, 0.5);
  m.p90Err = percentile(err, 0.9);
  m.phaseMedianErr = percentile(perr, 0.5);
  m.phaseP90Err = percentile(perr, 0.9);
  m.periodJumps = 100. * jumps / std::max<std::size_t>(1, perr.size());

  const auto& gt = sim.proj;
  const auto& k = obj.outputs.intrinsics.value;
  m.fxErr = std::abs(k[0] - gt.fx);
  m.cxErr = std::abs(k[2] - gt.cx);
  m.cyErr = std::abs(k[3] - gt.cy);
  const Eigen::Matrix4d M = procam::fromColumnMajor(obj.outputs.pose.value);
  m.posErrMm = 1000. * (M.topRightCorner<3, 1>() - gt.position()).norm();
  const Eigen::Matrix3d dR = M.topLeftCorner<3, 3>().transpose() * gt.R.transpose();
  m.rotErrDeg = std::acos(std::clamp((dR.trace() - 1.) / 2., -1., 1.)) * 180. / M_PI;
  m.rms = obj.outputs.rms.value;

  const Eigen::Matrix4d W = gt.worldFromLocal();
  double se = 0.;
  int n = 0;
  const auto& xyz = obj.outputs.xyz.texture;
  for(int v = 0; v < PH; v += 4)
    for(int u = 0; u < PW; u += 4)
    {
      const float* p = xyz.bytes + (std::size_t(v) * PW + u) * 4;
      if(p[3] <= 0.f)
        continue;
      const Eigen::Vector3d dl((u - gt.cx) / gt.fx, (v - gt.cy) / gt.fy, 1.);
      Eigen::Vector3d t;
      if(!hitCorner(W.topRightCorner<3, 1>(), W.topLeftCorner<3, 3>() * dl, t))
        continue;
      se += (Eigen::Vector3d(p[0], p[1], p[2]) - t).squaredNorm();
      n++;
    }
  m.xyzRmsMm = n ? 1000. * std::sqrt(se / n) : -1.;

  std::printf(
      "%-30s pat %2d P %3d | decoded %5.1f%% phase %5.1f%% | err med %6.2f p90 %6.2f px | "
      "phase med %6.3f p90 %6.3f | jumps %.3f%% | fx %6.2f cx %5.2f cy %5.2f | pos %6.2f mm "
      "rot %.3f deg | rms %.3f px | xyz %6.2f mm\n",
      c.name, m.patterns, m.period, m.decodedPct, m.phasePct, m.medianErr, m.p90Err,
      m.phaseMedianErr, m.phaseP90Err, m.periodJumps, m.fxErr, m.cxErr, m.cyErr,
      m.posErrMm, m.rotErrDeg, m.rms, m.xyzRmsMm);
  std::fflush(stdout);
  return m;
}
}

TEST_CASE("Phase shift: auto period and sequence length", "[threedim][structuredlight][phase]")
{
  auto s = sl::GraySequence::make(1920, 1080, 2);
  s.enablePhase(4, 0, 2.2f);
  CHECK(s.period == 8);
  CHECK(s.count() == 46 + 8);
  auto t = sl::GraySequence::make(1920, 1080, 16);
  t.enablePhase(8, 0, 2.2f);
  CHECK(t.skip == 3);
  CHECK(t.period == 64);
  CHECK(t.count() == 2 + 2 * (8 + 8) + 16);

  // The emitted sinusoid: code^gamma is the cosine.
  std::vector<uint8_t> px(1920 * 1080 * 4);
  const int k0 = t.grayCount();
  sl::renderPattern(t, k0, 1.f, px.data());
  for(int u : {0, 16, 32, 48, 100})
  {
    const double emitted = std::pow(px[u * 4] / 255., 2.2);
    CHECK(std::abs(emitted - (0.5 + 0.5 * std::cos(2. * M_PI * u / 64.))) < 0.02);
  }
  sl::renderPattern(t, k0 + 8, 1.f, px.data());
  CHECK(px[(std::size_t(32) * 1920 + 5) * 4] == px[(std::size_t(32) * 1920 + 900) * 4]);
}

TEST_CASE("Phase shift against Gray code on a resolution-limited bench", "[threedim][structuredlight][phase]")
{
  Sim sim;
  std::printf("\n");
  const auto gray = runConfig({"Gray only (min stripe 2)", false, 4, 0, 2}, sim);
  const auto gray16 = runConfig({"Gray only (min stripe 16)", false, 4, 0, 16}, sim);
  const auto ph4 = runConfig({"Gray+phase N=4 (stripe 16)", true, 4, 0, 16}, sim);
  const auto ph8 = runConfig({"Gray+phase N=8 (stripe 16)", true, 8, 0, 16}, sim);
  const auto ph4auto2 = runConfig({"Gray+phase N=4 (stripe 2)", true, 4, 0, 2}, sim);
  const auto ph4r2 = runConfig({"Gray+phase N=4, RANSAC 2 px", true, 4, 0, 16, 2.2f, 2.f}, sim);
  const auto grayr2 = runConfig({"Gray only, RANSAC 4 px", false, 4, 0, 2, 2.2f, 4.f}, sim);
  const auto graySmooth = runConfig(
      {"Gray only, Smooth measured", false, 4, 0, 2, 2.2f, 0.f,
       StructuredLightCalibration::SmoothMeasured},
      sim);
  const auto phSmooth = runConfig(
      {"Gray+phase N=4, Smooth measured", true, 4, 0, 16, 2.2f, 0.f,
       StructuredLightCalibration::SmoothMeasured},
      sim);
  CHECK(phSmooth.xyzRmsMm < graySmooth.xyzRmsMm);
  (void)ph4r2;
  (void)grayr2;

  CHECK(gray.medianErr > 1.0);
  CHECK(ph4.phasePct > 80.);
  CHECK(ph4.phaseMedianErr < 0.5);
  CHECK(ph8.phaseMedianErr < 0.5);
  CHECK(ph4.medianErr < gray.medianErr / 3.);
  CHECK(ph4.periodJumps < 0.1);
  CHECK(ph8.periodJumps < 0.1);
  CHECK(ph4.rms < gray.rms);
  CHECK(ph4.rms < gray.rms / 4.);
  CHECK(ph4.posErrMm < 2.);
  CHECK(ph4.fxErr < 2.);
  CHECK(ph4.rotErrDeg < 0.05);
  (void)gray16;
  (void)ph4auto2;
}

TEST_CASE("Phase shift with an uncompensated projector gamma", "[threedim][structuredlight][phase]")
{
  Sim sim;
  sim.noise = 0.f;
  std::printf("\n");
  const auto ok4 = runConfig({"N=4 compensated (2.2)", true, 4, 0, 16, 2.2f}, sim);
  const auto bad3 = runConfig({"N=3 uncompensated (1.0)", true, 3, 0, 16, 1.0f}, sim);
  const auto bad4 = runConfig({"N=4 uncompensated (1.0)", true, 4, 0, 16, 1.0f}, sim);
  const auto bad8 = runConfig({"N=8 uncompensated (1.0)", true, 8, 0, 16, 1.0f}, sim);

  // An uncompensated gamma turns the emitted cosine into f = (1/2 + 1/2 cos)^g.
  // N steps alias its harmonics N - 1 and N + 1 onto the fundamental, a phase
  // ripple of amplitude |a(N+1) - a(N-1)| / a1 radians at N times the
  // frequency, whose median |error| is sin(pi/4) of that amplitude.
  auto harmonic = [](int h) {
    double a = 0.;
    const int n = 20000;
    for(int i = 0; i < n; i++)
    {
      const double t = 2. * M_PI * (i + 0.5) / n;
      a += std::pow(0.5 + 0.5 * std::cos(t), 2.2) * std::cos(h * t);
    }
    return 2. * a / n;
  };
  auto predicted = [&](int N, int P) {
    const double amp = std::abs(harmonic(N + 1) - harmonic(N - 1)) / harmonic(1);
    return std::sin(M_PI / 4.) * amp * P / (2. * M_PI);
  };
  const double p3 = predicted(3, bad3.period), p4 = predicted(4, bad4.period),
               p8 = predicted(8, bad8.period);
  std::printf(
      "predicted median phase error from the harmonics: N=3 %.3f px, N=4 %.4f px, N=8 "
      "%.6f px\n",
      p3, p4, p8);
  CHECK(std::abs(bad3.phaseMedianErr - p3) < 0.2 * p3 + ok4.phaseMedianErr);
  CHECK(bad3.phaseMedianErr > 10. * ok4.phaseMedianErr);
  CHECK(bad4.phaseMedianErr < ok4.phaseMedianErr + p4 + 0.05);
  CHECK(bad8.phaseMedianErr < ok4.phaseMedianErr + p8 + 0.02);
  CHECK(bad3.periodJumps < 0.1);
  CHECK(bad4.periodJumps < 0.1);
}

TEST_CASE("Locked intrinsics against a full solve biased by depth noise", "[threedim][structuredlight][intrinsics]")
{
  Sim sim;
  sim.depthNoise = 0.01f;
  sim.litFraction = 0.4f;
  std::printf("\n");
  const auto gt = sim.proj;
  const std::array<float, 4> k{float(gt.fx), float(gt.fy), float(gt.cx), float(gt.cy)};
  const std::array<float, 4> k1{float(gt.fx * 1.01), float(gt.fy * 1.01), float(gt.cx), float(gt.cy)};
  const auto solve = runConfig({"Solve, noisy depth, 40% lit", true, 4, 0, 16}, sim);
  Config lock{"Lock (true K)", true, 4, 0, 16};
  lock.kmode = StructuredLightCalibration::LockIntrinsics;
  lock.lockedK = k;
  const auto locked = runConfig(lock, sim);
  Config lockOff{"Lock (fx +1%)", true, 4, 0, 16};
  lockOff.kmode = StructuredLightCalibration::LockIntrinsics;
  lockOff.lockedK = k1;
  const auto lockedOff = runConfig(lockOff, sim);

  CHECK(locked.posErrMm < solve.posErrMm / 3.);
  CHECK(locked.rotErrDeg < solve.rotErrDeg);
  CHECK(locked.fxErr == 0.);
  // A wrong fx moves the pose along the optical axis by the same fraction of
  // the throw distance: 1 % at 2.6 m is about 26 mm.
  CHECK(lockedOff.posErrMm < 0.015 * 2600.);

  Sim bench;
  bench.depthNoise = 0.02f;
  bench.litFraction = 0.3f;
  const auto solveB = runConfig({"Solve, 2 cm depth noise, 30% lit", true, 4, 0, 16}, bench);
  const auto lockB = runConfig(lock, bench);
  const auto lockOffB = runConfig(lockOff, bench);
  CHECK(lockB.posErrMm < solveB.posErrMm / 3.);
  CHECK(lockB.fxErr == 0.);
  CHECK(lockOffB.posErrMm < solveB.posErrMm);
  CHECK(lockOffB.rotErrDeg < solveB.rotErrDeg);
}
