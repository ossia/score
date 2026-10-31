// Structured Light Calibration photometric phases on a simulated bench: an
// 8-bit camera with a non-power response whose exposure follows the Camera
// exposure outlet, a projector with a different gamma per channel and primary
// crosstalk, on a pink striped fabric. The run recovers the camera response,
// the projector response, and the surface compensation; the saved files then
// drive a simulated compensated projection. Also the Canvas output.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Threedim/StructuredLightCalibration.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

using Threedim::StructuredLightCalibration;
namespace procam = score::gfx::procam;
namespace ph = score::gfx::photometric;

namespace
{
constexpr int PW = 320, PH = 200;
constexpr int CW = 400, CH = 240;
constexpr float captureExposure = 100.f;
const double projGamma[3] = {2.0, 2.4, 2.2};
const double ambient[3] = {0.01, 0.01, 0.015};
bool dimMixtures = false;

//! A picture mode that boosts the primaries, acting on the input codes:
//! mixtures are dimmed, white to 0.6.
void mixLight(const Eigen::Vector3d& code, Eigen::Vector3d& light)
{
  if(dimMixtures && code.maxCoeff() > 0.)
    light *= 1. - 0.4 * code.minCoeff() / code.maxCoeff();
}

double cameraCurve(int c, double x)
{
  x = std::clamp(x, 0., 1.);
  const double a = 0.55 + 0.05 * c;
  return a * std::pow(x, 0.4) + (1. - a) * std::pow(x, 1.6);
}

double projectorLight(int c, double code)
{
  return 0.97 * std::pow(code, projGamma[c]) + 0.03 * code;
}

procam::Model projector()
{
  procam::Model m;
  m.fx = m.fy = 300.;
  m.cx = 159.5;
  m.cy = 99.5;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  m.R = flip.transpose();
  m.t = -m.R * Eigen::Vector3d(0., 1.2, 0.);
  return m;
}

bool hit(const Eigen::Vector3d& o, const Eigen::Vector3d& d, Eigen::Vector3d& p)
{
  double best = 1e9;
  if(d.z() < 0.)
    best = (-2. - o.z()) / d.z();
  if(d.y() < 0.)
    best = std::min(best, (0.75 - o.y()) / d.y());
  if(best > 1e8)
    return false;
  p = o + best * d;
  return true;
}

Eigen::Vector3d fabric(const Eigen::Vector3d& p)
{
  Eigen::Vector3d r(0.95, 0.62, 0.70);
  if(int(std::floor(p.x() / 0.12)) % 2)
    r *= 0.7;
  if(p.x() > 0.3 && p.y() > 1.3)
    r = Eigen::Vector3d(0.55, 0.65, 0.9);
  return r;
}

struct Bench
{
  procam::Model proj = projector();
  Eigen::Matrix3d mix;
  Eigen::Matrix4d worldFromCamera = Eigen::Matrix4d::Identity();
  double fx = 260., fy = 260., cx = 199.5, cy = 119.5;
  std::vector<Eigen::Vector3d> world;
  std::vector<uint8_t> has;
  std::vector<float> image, positions;
  float exposure{captureExposure};

  Bench()
  {
    mix << 0.80, 0.10, 0.02, 0.12, 0.85, 0.08, 0.03, 0.10, 0.75;
    const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
    worldFromCamera.topLeftCorner<3, 3>() = flip;
    worldFromCamera.topRightCorner<3, 1>() = Eigen::Vector3d(0., 1.3, 0.3);
    world.resize(CW * CH);
    has.assign(CW * CH, 0);
    image.assign(CW * CH * 4, 0.f);
    positions.assign(CW * CH * 4, 0.f);
    const Eigen::Vector3d o = worldFromCamera.topRightCorner<3, 1>();
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        const Eigen::Vector3d d = flip * Eigen::Vector3d((x - cx) / fx, (y - cy) / fy, 1.);
        Eigen::Vector3d p;
        if(!hit(o, d, p))
          continue;
        const int i = y * CW + x;
        world[i] = p;
        has[i] = 1;
        const Eigen::Vector3d pc = flip * (p - o);
        positions[i * 4 + 0] = float(pc.x());
        positions[i * 4 + 1] = float(pc.y());
        positions[i * 4 + 2] = float(pc.z());
        positions[i * 4 + 3] = 1.f;
      }
  }

  double vignette(double u, double v) const
  {
    const double dx = (u - proj.cx) / PW, dy = (v - proj.cy) / PH;
    return 1. - 0.5 * (dx * dx + dy * dy);
  }

  //! Linear light the camera sees at pixel i for projector light `light` (rgb).
  Eigen::Vector3d seen(int i, const Eigen::Vector3d& light, double u, double v) const
  {
    return Eigen::Vector3d(ambient[0], ambient[1], ambient[2])
           + 0.8 * vignette(u, v) * fabric(world[i]).cwiseProduct(mix * light);
  }

  void render(const uint8_t* pattern, int pw, int ph)
  {
    const double e = exposure / captureExposure;
    for(int i = 0; i < CW * CH; i++)
    {
      Eigen::Vector3d L(ambient[0], ambient[1], ambient[2]);
      Eigen::Vector2d px;
      if(has[i] && pattern && proj.project(world[i], px))
      {
        const int u = int(std::floor(px.x() + 0.5)), v = int(std::floor(px.y() + 0.5));
        if(u >= 0 && v >= 0 && u < pw && v < ph)
        {
          const uint8_t* q = pattern + (std::size_t(v) * pw + u) * 4;
          Eigen::Vector3d light;
          for(int c = 0; c < 3; c++)
            light[c] = projectorLight(c, q[c] / 255.);
          mixLight(Eigen::Vector3d(q[0], q[1], q[2]) / 255., light);
          L = seen(i, light, u, v);
        }
      }
      for(int c = 0; c < 3; c++)
        image[i * 4 + c] = std::round(255.f * float(cameraCurve(c, L[c] * e))) / 255.f;
      image[i * 4 + 3] = 1.f;
    }
  }
};

bool run(StructuredLightCalibration& obj, Bench& b, int* exposureChanges)
{
  double now = 1000.;
  obj.tick(now);
  obj.inputs.start.value = halp::impulse_type{};
  for(int i = 0; i < 60000; i++)
  {
    auto& p1 = obj.outputs.pattern.texture;
    b.render(p1.bytes, p1.width, p1.height);
    obj.inputs.camera.texture.bytes
        = obj.inputs.camera.request_readback ? b.image.data() : nullptr;
    obj.inputs.camera.texture.width = CW;
    obj.inputs.camera.texture.height = CH;
    obj.inputs.positions.texture.bytes
        = obj.inputs.positions.request_readback ? b.positions.data() : nullptr;
    obj.inputs.positions.texture.width = CW;
    obj.inputs.positions.texture.height = CH;
    now += 16.7;
    obj.tick(now);
    obj.inputs.start.value.reset();
    if(obj.outputs.camera_exposure.value)
    {
      b.exposure = *obj.outputs.camera_exposure.value;
      (*exposureChanges)++;
    }
    if(obj.outputs.done.value)
      return true;
    if(!obj.running())
      return false;
    if(obj.outputs.status.value == "Decoding")
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}
}

TEST_CASE("Structured light: camera response, projector response and compensation", "[threedim][structuredlight][photometric]")
{
  Bench b;
  StructuredLightCalibration obj;
  obj.inputs.projector_size.value = {PW, PH};
  obj.inputs.mode.value = StructuredLightCalibration::DepthCamera;
  obj.inputs.world_from_camera.value = procam::toColumnMajor(b.worldFromCamera);
  obj.inputs.intrinsics.value = {float(b.fx), float(b.fy), float(b.cx), float(b.cy)};
  obj.inputs.settle.value = 50;
  obj.inputs.frames.value = 1;
  obj.inputs.hole_radius.value = 4;
  obj.inputs.surface.value = StructuredLightCalibration::RawMeasured;
  obj.inputs.camera_gamma.value = 2.2f;
  obj.inputs.response_levels.value = 8;
  obj.inputs.compensation.value = true;
  obj.inputs.exposures.value = "12.5 25 50 100 200 400 800";
  obj.inputs.exposure_settle.value = 100;
  obj.inputs.capture_exposure.value = captureExposure;

  QTemporaryDir autoDir;
  obj.inputs.auto_save.value = true;
  obj.inputs.save_path.value = autoDir.filePath("auto").toStdString();

  int changes = 0;
  REQUIRE(run(obj, b, &changes));
  INFO(obj.outputs.status.value);
  for(const char* f : {"auto.json", "auto.xyz.pfm", "auto.response.json", "auto.comp.json",
                       "auto.crf.json"})
    CHECK(QFile::exists(autoDir.filePath(f)));
  CHECK(b.exposure == captureExposure);
  CHECK(changes >= 8);

  const auto* r = obj.lastResult();
  REQUIRE(r);
  REQUIRE(r->crf);
  REQUIRE(r->response);

  // Camera response: radiance ratios against the true curve.
  double crfErr = 0.;
  for(int c = 0; c < 3; c++)
    for(double v = 0.1; v <= 0.9; v += 0.05)
    {
      auto inv = [&](double t) {
        double lo = 0., hi = 1.;
        for(int it = 0; it < 60; it++)
          (cameraCurve(c, 0.5 * (lo + hi)) < t ? lo : hi) = 0.5 * (lo + hi);
        return 0.5 * (lo + hi);
      };
      const double truth = inv(v) / inv(0.5);
      const double est = r->crf->linear(c, v) / r->crf->linear(c, 0.5);
      crfErr = std::max(crfErr, std::abs(est / truth - 1.));
    }
  INFO("camera response: max radiance ratio error " << crfErr);
  CHECK(crfErr < 0.06);

  // Projector response through the recovered camera response.
  double respErr = 0.;
  for(int c = 0; c < 3; c++)
    for(int i = 0; i <= 20; i++)
    {
      const double t = i / 20.;
      respErr = std::max(
          respErr, std::abs((*r->response).channels[c](t) - projectorLight(c, t)));
    }
  const auto& g = obj.outputs.measured_gamma.value;
  INFO("measured gamma " << g[0] << " " << g[1] << " " << g[2] << ", max response error "
                         << respErr << ", additivity " << r->response->additivityError);
  CHECK(std::abs(g[0] - 2.0) < 0.2);
  CHECK(std::abs(g[1] - 2.4) < 0.2);
  CHECK(std::abs(g[2] - 2.2) < 0.2);
  CHECK(respErr < 0.04);
  CHECK(r->compScale > 0.f);

  QTemporaryDir dir;
  obj.inputs.save_path.value = dir.filePath("room").toStdString();
  obj.inputs.save.value = halp::impulse_type{};
  obj.tick(1e9);
  obj.inputs.save.value.reset();
  INFO(obj.outputs.status.value);
  for(const char* f : {"room.crf.json", "room.response.json", "room.response.cube",
                       "room.comp.json", "room.comp0.pfm", "room.compoffset.pfm"})
    CHECK(QFile::exists(dir.filePath(f)));

  const auto table = score::gfx::loadResponseTable(dir.filePath("room.response.json"));
  const auto cube = score::gfx::loadResponseTable(dir.filePath("room.response.cube"));
  const auto comp = score::gfx::loadCompensation(dir.filePath("room.comp.json"));
  REQUIRE(table.valid());
  REQUIRE(cube.valid());
  REQUIRE(comp.valid());
  CHECK(cube.size == table.size);
  CHECK(cube.inverse[512 * 3 + 1] == Catch::Approx(table.inverse[512 * 3 + 1]).margin(1e-5));
  auto encode = [&](int c, double t) {
    const double f = std::clamp(t, 0., 1.) * (table.size - 1);
    const int i0 = std::min(int(f), table.size - 2);
    const double a = f - i0;
    return table.inverse[i0 * 3 + c] * (1. - a) + table.inverse[(i0 + 1) * 3 + c] * a;
  };

  // A compensated projection as the multi-window output computes it:
  // code = encode(M c + o), against the same content projected as is
  // (code = c ^ (1 / 2.2) of the linear content c).
  struct Sample
  {
    Eigen::Vector3d before, after, target;
  };
  std::vector<Sample> samples;
  const Eigen::Matrix4d Wp = b.proj.worldFromLocal();
  for(int v = 10; v < PH - 10; v += 3)
    for(int u = 10; u < PW - 10; u += 3)
    {
      Eigen::Vector3d X;
      if(!hit(
             Wp.topRightCorner<3, 1>(),
             Wp.topLeftCorner<3, 3>()
                 * Eigen::Vector3d((u - b.proj.cx) / b.proj.fx, (v - b.proj.cy) / b.proj.fy, 1.),
             X))
        continue;
      Eigen::Vector2d cpx;
      const Eigen::Matrix4d Cw = b.worldFromCamera.inverse();
      const Eigen::Vector3d xc = (Cw * X.homogeneous()).head<3>();
      if(xc.z() <= 0.)
        continue;
      const int cxp = int(std::lround(b.fx * xc.x() / xc.z() + b.cx));
      const int cyp = int(std::lround(b.fy * xc.y() / xc.z() + b.cy));
      if(cxp < 0 || cyp < 0 || cxp >= CW || cyp >= CH)
        continue;
      const int ci = cyp * CW + cxp;
      if(!b.has[ci] || (b.world[ci] - X).norm() > 0.01)
        continue;
      const Eigen::Vector3d c(
          0.2 + 0.6 * u / double(PW), 0.2 + 0.6 * v / double(PH), 0.5 + 0.3 * std::sin(u * 0.05));
      const std::size_t k = std::size_t(v) * PW + u;
      Eigen::Vector3d p;
      for(int a = 0; a < 3; a++)
      {
        const float* row = comp.rows[a].data.data() + k * 4;
        p[a] = std::clamp(row[0] * c[0] + row[1] * c[1] + row[2] * c[2] + row[3], 0., 1.);
      }
      Eigen::Vector3d lightAfter, lightBefore;
      for(int a = 0; a < 3; a++)
      {
        lightAfter[a] = projectorLight(a, encode(a, p[a]));
        lightBefore[a] = projectorLight(a, std::pow(c[a], 1. / 2.2));
      }
      samples.push_back({b.seen(ci, lightBefore, u, v), b.seen(ci, lightAfter, u, v), c});
    }
  REQUIRE(samples.size() > 1000);
  auto score = [&](bool after, double& psnr, double& dE) {
    Eigen::Vector3d num = Eigen::Vector3d::Zero(), den = Eigen::Vector3d::Zero();
    for(auto& s : samples)
    {
      const Eigen::Vector3d img = (after ? s.after : s.before)
                                  - Eigen::Vector3d(ambient[0], ambient[1], ambient[2]);
      num += img.cwiseProduct(s.target);
      den += img.cwiseProduct(img);
    }
    const Eigen::Vector3d gain = num.cwiseQuotient(den);
    std::vector<double> a, t;
    dE = 0.;
    for(auto& s : samples)
    {
      const Eigen::Vector3d img = ((after ? s.after : s.before)
                                   - Eigen::Vector3d(ambient[0], ambient[1], ambient[2]))
                                      .cwiseProduct(gain);
      for(int c = 0; c < 3; c++)
      {
        a.push_back(std::pow(std::clamp(img[c], 0., 1.), 1. / 2.2));
        t.push_back(std::pow(std::clamp(s.target[c], 0., 1.), 1. / 2.2));
      }
      dE += ph::deltaE76({img[0], img[1], img[2]}, {s.target[0], s.target[1], s.target[2]});
    }
    psnr = ph::psnr(a, t);
    dE /= samples.size();
  };
  double p0, e0, p1, e1;
  score(false, p0, e0);
  score(true, p1, e1);
  INFO("on " << samples.size() << " projector pixels, compensation white " << r->compScale);
  INFO("uncompensated: PSNR " << p0 << " dB, mean dE76 " << e0);
  INFO("compensated through the saved files: PSNR " << p1 << " dB, mean dE76 " << e1);
  CHECK(p1 > p0 + 8.);
  CHECK(e1 < 3.);

  // The saved camera response is loaded back and linearises the same way.
  const auto crf = StructuredLightCalibration::loadCameraResponse(
      dir.filePath("room.crf.json").toStdString());
  REQUIRE(crf);
  CHECK(crf->linear(1, 0.3) == Catch::Approx(r->crf->linear(1, 0.3)).epsilon(1e-6));
}

TEST_CASE("Canvas: every projector's pattern in its layout rect", "[threedim][structuredlight]")
{
  StructuredLightCalibration obj;
  obj.inputs.projector_size.value = {64, 32};
  obj.inputs.projector_count.value = 2;
  obj.inputs.idle.value = StructuredLightCalibration::TestGrid;
  obj.inputs.canvas_size.value = {128, 32};
  obj.tick(0.);
  const auto& cv = obj.outputs.canvas.texture;
  const auto& p1 = obj.outputs.pattern.texture;
  const auto& p2 = obj.outputs.pattern2.texture;
  REQUIRE(cv.width == 128);
  REQUIRE(cv.height == 32);
  int diff = 0;
  for(int y = 0; y < 32; y++)
    for(int x = 0; x < 64; x++)
      for(int c = 0; c < 4; c++)
      {
        diff += cv.bytes[(y * 128 + x) * 4 + c] != p1.bytes[(y * 64 + x) * 4 + c];
        diff += cv.bytes[(y * 128 + 64 + x) * 4 + c] != p2.bytes[(y * 64 + x) * 4 + c];
      }
  CHECK(diff == 0);

  obj.inputs.canvas_layout.value = "0.5 0 0.5 1; 0 0 0.5 1";
  obj.tick(1.);
  diff = 0;
  for(int y = 0; y < 32; y++)
    for(int x = 0; x < 64; x++)
      diff += cv.bytes[(y * 128 + 64 + x) * 4] != p1.bytes[(y * 64 + x) * 4];
  CHECK(diff == 0);

  const auto rects = StructuredLightCalibration::canvasRects("", 3);
  REQUIRE(rects.size() == 3);
  CHECK(rects[2][0] == Catch::Approx(2. / 3.));
  CHECK(rects[2][2] == Catch::Approx(1. / 3.));
}

TEST_CASE("Structured light: projector colour mixing", "[threedim][structuredlight][photometric]")
{
  dimMixtures = true;
  struct Reset
  {
    ~Reset() { dimMixtures = false; }
  } reset;
  Bench b;
  StructuredLightCalibration obj;
  obj.inputs.projector_size.value = {PW, PH};
  obj.inputs.mode.value = StructuredLightCalibration::DepthCamera;
  obj.inputs.world_from_camera.value = procam::toColumnMajor(b.worldFromCamera);
  obj.inputs.intrinsics.value = {float(b.fx), float(b.fy), float(b.cx), float(b.cy)};
  obj.inputs.settle.value = 50;
  obj.inputs.frames.value = 1;
  obj.inputs.hole_radius.value = 4;
  obj.inputs.surface.value = StructuredLightCalibration::RawMeasured;
  obj.inputs.response_levels.value = 8;
  obj.inputs.compensation.value = true;
  obj.inputs.exposures.value = "12.5 25 50 100 200 400 800";
  obj.inputs.exposure_settle.value = 100;
  obj.inputs.capture_exposure.value = captureExposure;

  int changes = 0;
  REQUIRE(run(obj, b, &changes));
  INFO(obj.outputs.status.value);
  const auto* r = obj.lastResult();
  REQUIRE(r);
  REQUIRE(r->response);
  const auto& mx = r->response->mixing;
  REQUIRE(mx.valid());
  const auto white = mx.forward({1., 1., 1.});
  for(int c = 0; c < 3; c++)
    CHECK(white[c] == Catch::Approx(0.6).margin(0.05));
  CHECK(r->response->mixErrorModel < 0.03);
  CHECK(r->response->mixErrorAdditive > 3. * r->response->mixErrorModel);

  QTemporaryDir dir;
  obj.inputs.save_path.value = dir.filePath("mix").toStdString();
  obj.inputs.save.value = halp::impulse_type{};
  obj.tick(1e9);
  obj.inputs.save.value.reset();
  const auto table = score::gfx::loadResponseTable(dir.filePath("mix.response.json"));
  REQUIRE(table.valid());
  REQUIRE(table.mixing.valid());
  const auto comp = score::gfx::loadCompensation(dir.filePath("mix.comp.json"));
  REQUIRE(comp.valid());

  auto encode = [&](int c, double t) {
    const double f = std::clamp(t, 0., 1.) * (table.size - 1);
    const int i0 = std::min(int(f), table.size - 2);
    const double a = f - i0;
    return table.inverse[i0 * 3 + c] * (1. - a) + table.inverse[(i0 + 1) * 3 + c] * a;
  };
  // As the output stage computes it: e = M c, l = mixing table (e), code =
  // encode(l). The light reaching the surface, through the primaries'
  // crosstalk, must be proportional to the content per channel.
  const std::size_t k = std::size_t(PH / 2) * PW + PW / 2;
  auto shown = [&](const Eigen::Vector3d& c) {
    std::array<double, 3> e;
    for(int a = 0; a < 3; a++)
    {
      const float* row = comp.rows[a].data.data() + k * 4;
      e[a] = row[0] * c[0] + row[1] * c[1] + row[2] * c[2] + row[3];
    }
    const auto l = table.mixing(e);
    Eigen::Vector3d code, light;
    for(int a = 0; a < 3; a++)
    {
      code[a] = std::round(255. * encode(a, l[a])) / 255.;
      light[a] = projectorLight(a, code[a]);
    }
    mixLight(code, light);
    return Eigen::Vector3d(b.mix * light);
  };
  const Eigen::Vector3d full = shown(Eigen::Vector3d::Ones());
  for(const Eigen::Vector3d c :
      {Eigen::Vector3d(0.5, 0.5, 0.5), Eigen::Vector3d(1., 0.5, 0.2), Eigen::Vector3d(0.3, 0.8, 0.6),
       Eigen::Vector3d(1., 1., 0.3)})
  {
    const Eigen::Vector3d got = shown(c).cwiseQuotient(full);
    INFO("content " << c.transpose() << " shown " << got.transpose());
    CHECK((got - c).cwiseAbs().maxCoeff() < 0.05);
  }
}
