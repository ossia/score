// LED walls (flat and cabinet XYZ maps), LED discovery by binary blinking on a
// simulated camera with blur, noise and a depth camera, and the head-tracking
// viewer filter.

#include <Gfx/Calibration/LedMapping.hpp>
#include <Gfx/Calibration/OffAxis.hpp>
#include <Threedim/LedDiscovery.hpp>
#include <Threedim/LedWall.hpp>
#include <Threedim/ViewerFromPose.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

namespace led = score::gfx::led;
using Catch::Approx;

TEST_CASE("LED wall: flat wall from its corners", "[led]")
{
  Threedim::LedWallMap w;
  w.inputs.mode.value = Threedim::LedWallMap::Flat;
  w.inputs.size.value = {64, 36};
  w.inputs.tl.value = {-2.f, 2.25f, -3.f};
  w.inputs.tr.value = {2.f, 2.25f, -3.f};
  w.inputs.bl.value = {-2.f, 0.f, -3.f};
  w.inputs.br.value = {2.f, 0.f, -3.f};
  w();
  INFO(w.outputs.status.value);
  REQUIRE(w.outputs.xyz.texture.width == 64);
  const float* p = w.outputs.xyz.texture.bytes;
  // Pixel (0, 0) centre: 1/128 of the width in from the left, 1/72 down from the top.
  CHECK(p[0] == Approx(-2. + 4. / 128.));
  CHECK(p[1] == Approx(2.25 - 2.25 / 72.));
  CHECK(p[2] == Approx(-3.));
  CHECK(p[3] == 1.f);
  const float* q = p + (35 * 64 + 63) * 4;
  CHECK(q[0] == Approx(2. - 4. / 128.));
  CHECK(q[1] == Approx(2.25 / 72.));
  CHECK(w.outputs.pa.value.x == Approx(-2.f));
  CHECK(w.outputs.pa.value.y == Approx(0.f).margin(1e-6));
  CHECK(w.outputs.pc.value.y == Approx(2.25f));
  CHECK(w.outputs.pb.value.x == Approx(2.f));
  CHECK(w.outputs.positions.value.size() == 64 * 36 * 4);
}

TEST_CASE("LED wall: curved wall of cabinets with gaps", "[led]")
{
  QTemporaryDir dir;
  const QString path = dir.filePath("cabinets.csv");
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("# x, y, w, h, TL, TR, BL, BR\n");
    f.write("x,y,w,h,tlx,tly,tlz,trx,try,trz,blx,bly,blz,brx,bry,brz\n");
    // Four 32 x 32 cabinets on an arc of radius 3, a 4 px gap after the second.
    for(int i = 0; i < 4; i++)
    {
      const double a0 = -0.4 + 0.2 * i, a1 = a0 + 0.2;
      const int x = i * 32 + (i >= 2 ? 4 : 0);
      char line[400];
      std::snprintf(
          line, sizeof(line), "%d, %d, 32, 32, %f,1,%f, %f,1,%f, %f,0,%f, %f,0,%f\n", x, 0,
          3 * std::sin(a0), -3 * std::cos(a0), 3 * std::sin(a1), -3 * std::cos(a1),
          3 * std::sin(a0), -3 * std::cos(a0), 3 * std::sin(a1), -3 * std::cos(a1));
      f.write(line);
    }
  }
  std::string err;
  QFile f{path};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto text = f.readAll().toStdString();
  const auto mods = led::parseModules(text, &err);
  REQUIRE(mods.size() == 4);
  const auto sz = led::rasterSize(mods);
  CHECK(sz[0] == 132);
  CHECK(sz[1] == 32);

  Threedim::LedWallMap w;
  w.inputs.mode.value = Threedim::LedWallMap::Cabinets;
  w.inputs.csv.value = path.toStdString();
  w();
  INFO(w.outputs.status.value);
  REQUIRE(w.outputs.xyz.texture.width == 132);
  const float* m = w.outputs.xyz.texture.bytes;
  int gap = 0, lit = 0;
  double maxRadiusErr = 0.;
  for(int y = 0; y < 32; y++)
    for(int x = 0; x < 132; x++)
    {
      const float* p = m + (y * 132 + x) * 4;
      if(x >= 64 && x < 68)
      {
        gap += p[3] == 0.f;
        continue;
      }
      lit += p[3] > 0.f;
      // Chords of the arc: within the sagitta of 3 (1 - cos 0.1).
      maxRadiusErr = std::max(maxRadiusErr, 3. - std::hypot(p[0], p[2]));
    }
  CHECK(gap == 4 * 32);
  CHECK(lit == 128 * 32);
  CHECK(maxRadiusErr <= 3. * (1. - std::cos(0.1)) + 1e-5);
  CHECK(w.outputs.screens.value.size() == 4 * 9);

  // The bounding screen lies in the plane of the cabinets, along their rows.
  const auto& s = w.outputs;
  CHECK(s.pa.value.y == Approx(0.f).margin(1e-4));
  CHECK(s.pc.value.y == Approx(1.f).margin(1e-4));
  CHECK(s.pa.value.x < -1.1f);
  CHECK(s.pb.value.x > 1.1f);

  // A curved wall's tight frustum from a viewer comes from its XYZ map.
  const std::vector<float> map(m, m + 132 * 32 * 4);
  const score::gfx::offaxis::Vec3 eye(0., 0.5, 0.);
  const auto vf = score::gfx::offaxis::fitViewerFrustum(map, 132, 32, eye, 0.01, 1.);
  REQUIRE(vf.ok);
  CHECK_FALSE(vf.planar);
  const auto f2 = score::gfx::offaxis::kooima(vf.pa, vf.pb, vf.pc, eye);
  const Eigen::Matrix4d PV = f2.projection(0.1, 100.) * f2.view(eye);
  double worst = 0.;
  for(int i = 0; i < 132 * 32; i++)
  {
    const float* p = m + i * 4;
    if(p[3] <= 0.f)
      continue;
    const Eigen::Vector4d c = PV * Eigen::Vector4d(p[0], p[1], p[2], 1.);
    worst = std::max({worst, std::abs(c.x() / c.w()), std::abs(c.y() / c.w())});
  }
  CHECK(worst <= 1. + 1e-9);
  CHECK(worst > 0.99);
}

namespace
{
// Simulated LED fixture seen by a camera: Gaussian spots, noise, ambient.
struct LedBench
{
  int W = 320, H = 240, N = 300;
  std::vector<double> lu, lv, gain;
  std::vector<Eigen::Vector3d> world;
  std::vector<float> image, positions;
  std::mt19937 rng{7};

  LedBench()
  {
    std::uniform_real_distribution<double> u(10., 310.), v(10., 230.), g(0.3, 1.);
    // A random 3D strip in front of a camera at the origin looking along +z (OpenCV).
    for(int i = 0; i < N; i++)
    {
      double x, y;
      bool ok;
      do
      {
        x = u(rng);
        y = v(rng);
        ok = true;
        for(int j = 0; j < i && ok; j++)
          ok = std::hypot(x - lu[j], y - lv[j]) > 6.;
      } while(!ok);
      lu.push_back(x);
      lv.push_back(y);
      gain.push_back(g(rng));
      const double z = 2. + 0.5 * std::sin(i * 0.1);
      world.emplace_back((x - 159.5) / 250. * z, (y - 119.5) / 250. * z, z);
    }
    image.assign(W * H * 4, 0.f);
    positions.assign(W * H * 4, 0.f);
    for(int i = 0; i < N; i++)
      for(int y = int(lv[i]) - 2; y <= int(lv[i]) + 2; y++)
        for(int x = int(lu[i]) - 2; x <= int(lu[i]) + 2; x++)
        {
          float* p = positions.data() + (y * W + x) * 4;
          p[0] = float(world[i].x());
          p[1] = float(world[i].y());
          p[2] = float(world[i].z());
          p[3] = 1.f;
        }
  }
  void render(const std::vector<float>& values)
  {
    std::normal_distribution<float> noise(0.f, 0.01f);
    std::vector<float> lum(W * H, 0.03f);
    for(int i = 0; i < N && i < int(values.size()); i++)
    {
      if(values[i] <= 0.f)
        continue;
      for(int y = int(lv[i]) - 4; y <= int(lv[i]) + 4; y++)
        for(int x = int(lu[i]) - 4; x <= int(lu[i]) + 4; x++)
        {
          if(x < 0 || y < 0 || x >= W || y >= H)
            continue;
          const double r2 = std::pow(x - lu[i], 2) + std::pow(y - lv[i], 2);
          lum[y * W + x] += float(values[i] * gain[i] * std::exp(-r2 / (2. * 1.2 * 1.2)));
        }
    }
    for(int i = 0; i < W * H; i++)
    {
      const float l = std::clamp(lum[i] + noise(rng), 0.f, 1.f);
      image[i * 4] = image[i * 4 + 1] = image[i * 4 + 2] = l;
      image[i * 4 + 3] = 1.f;
    }
  }
};
}

TEST_CASE("LED discovery: binary blink decode on a noisy camera", "[led]")
{
  CHECK(led::BlinkCode::make(300).bits == 9);
  CHECK(led::BlinkCode::make(300).frames() == 20);
  CHECK(led::BlinkCode::make(1).bits == 1);

  LedBench b;
  Threedim::LedDiscovery d;
  d.inputs.count.value = b.N;
  d.inputs.settle.value = 50;
  d.inputs.frames.value = 2;
  d.inputs.min_signal.value = 0.08f;
  d.inputs.min_pixels.value = 2;
  d.inputs.world_from_camera.value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  d.inputs.camera.texture.bytes = nullptr;
  d.inputs.positions.texture.bytes = nullptr;
  double now = 0.;
  d.tick(now);
  d.inputs.start.value = halp::impulse_type{};
  bool done = false;
  for(int i = 0; i < 5000 && !done; i++)
  {
    b.render(d.outputs.values.value);
    d.inputs.camera.texture.bytes = d.inputs.camera.request_readback ? b.image.data() : nullptr;
    d.inputs.camera.texture.width = b.W;
    d.inputs.camera.texture.height = b.H;
    d.inputs.positions.texture.bytes
        = d.inputs.positions.request_readback ? b.positions.data() : nullptr;
    d.inputs.positions.texture.width = b.W;
    d.inputs.positions.texture.height = b.H;
    now += 16.7;
    d.tick(now);
    d.inputs.start.value.reset();
    done = bool(d.outputs.done.value);
  }
  INFO(d.outputs.status.value);
  REQUIRE(done);
  int found = 0;
  double maxErr = 0., sumErr = 0., maxWorld = 0.;
  for(const auto& l : d.leds())
  {
    if(!l.found)
      continue;
    found++;
    const double e = std::hypot(l.u - b.lu[l.index], l.v - b.lv[l.index]);
    maxErr = std::max(maxErr, e);
    sumErr += e;
    if(l.hasWorld)
      maxWorld = std::max(
          maxWorld, (Eigen::Vector3d(l.x, l.y, l.z) - b.world[l.index]).norm());
  }
  INFO("found " << found << " / " << b.N << ", position error mean " << sumErr / found
                << " px, max " << maxErr << " px, world max " << maxWorld << " m");
  CHECK(found >= b.N * 0.98);
  CHECK(maxErr < 0.5);
  CHECK(sumErr / found < 0.2);
  CHECK(maxWorld < 1e-4);
  CHECK(d.outputs.uv.value.size() == std::size_t(b.N) * 4);
  CHECK(d.outputs.colours.value.size() == std::size_t(b.N) * 3);

  QTemporaryDir dir;
  const auto csv = dir.filePath("leds.csv").toStdString();
  REQUIRE(Threedim::LedDiscovery::writeCsv(csv, d.leds()));
  QFile f{QString::fromStdString(csv)};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.readAll().count('\n') == b.N + 1);
}

TEST_CASE("Viewer from pose: transform, then the 1 euro filter", "[viewer]")
{
  Threedim::ViewerFromPose v;
  v.inputs.scale.value = 0.001f;
  v.inputs.transform.value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.5f, 0, 0, 1};
  v.inputs.offset.value = {0.f, 0.1f, 0.f};
  v.inputs.min_cutoff.value = 1.f;
  v.inputs.beta.value = 0.f;
  std::mt19937 rng{3};
  std::normal_distribution<float> jitter(0.f, 5.f);
  double t = 0.;
  double rawVar = 0., outVar = 0.;
  int n = 0;
  for(int i = 0; i < 600; i++)
  {
    t += 1. / 60.;
    v.clockOverride = t;
    v.inputs.position.value = {1000.f + jitter(rng), 1600.f + jitter(rng), 0.f};
    v();
    if(i > 120)
    {
      rawVar += std::pow(v.outputs.raw.value.x - 1.5, 2);
      outVar += std::pow(v.outputs.viewer.value.x - 1.5, 2);
      n++;
    }
  }
  INFO("jitter raw " << std::sqrt(rawVar / n) * 1000. << " mm, filtered "
                     << std::sqrt(outVar / n) * 1000. << " mm");
  CHECK(v.outputs.raw.value.y == Approx(1.7f).margin(0.03f));
  CHECK(std::sqrt(outVar / n) < 0.3 * std::sqrt(rawVar / n));

  // With beta, a fast move is followed with less lag than with beta = 0.
  auto lagAfterStep = [](float beta) {
    Threedim::ViewerFromPose f;
    f.inputs.min_cutoff.value = 1.f;
    f.inputs.beta.value = beta;
    double t = 0.;
    for(int i = 0; i < 60; i++)
    {
      t += 1. / 60.;
      f.clockOverride = t;
      f.inputs.position.value = {0.f, 0.f, 0.f};
      f();
    }
    for(int i = 0; i < 6; i++)
    {
      t += 1. / 60.;
      f.clockOverride = t;
      f.inputs.position.value = {float(0.2 * (i + 1) / 6.), 0.f, 0.f};
      f();
    }
    return f.outputs.raw.value.x - f.outputs.viewer.value.x;
  };
  const float lag0 = lagAfterStep(0.f), lag1 = lagAfterStep(4.f);
  INFO("lag after 100 ms of a 2 m/s move: beta 0 " << lag0 << " m, beta 4 " << lag1 << " m");
  CHECK(lag1 < 0.6f * lag0);
}
