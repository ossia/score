// Structured Light Calibration, end to end on a simulated bench: a projector
// and a camera (OpenCV pinholes) look at a two-wall corner with a floor; every
// tick the camera image is rendered from the pattern the process outputs, and
// the process is driven through its wall-clock state machine.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/StructuredLightCalibration.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <cmath>
#include <thread>
#include <vector>

using Catch::Approx;
using Threedim::StructuredLightCalibration;
namespace procam = score::gfx::procam;
namespace sl = Threedim::sl;

namespace
{
constexpr int PW = 640, PH = 360;
constexpr int CW = 320, CH = 240;

procam::Model projector()
{
  procam::Model m;
  m.fx = 700.;
  m.fy = 690.;
  m.cx = 330.;
  m.cy = 170.;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  const Eigen::Matrix3d wR = (Eigen::AngleAxisd(0.25, Eigen::Vector3d::UnitY())
                              * Eigen::AngleAxisd(-0.2, Eigen::Vector3d::UnitX()))
                                 .toRotationMatrix()
                             * flip;
  const Eigen::Vector3d pos(0.3, 1.3, 2.2);
  m.R = wR.transpose();
  m.t = -m.R * pos;
  return m;
}

struct CameraSim
{
  double fx = 230., fy = 230., cx = 159.5, cy = 119.5;
  Eigen::Matrix4d worldFromCamera;

  CameraSim()
  {
    worldFromCamera = Eigen::Matrix4d::Identity();
    const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
    worldFromCamera.topLeftCorner<3, 3>()
        = (Eigen::AngleAxisd(0.3, Eigen::Vector3d::UnitY())
           * Eigen::AngleAxisd(-0.25, Eigen::Vector3d::UnitX()))
              .toRotationMatrix()
          * flip;
    worldFromCamera.topRightCorner<3, 1>() = Eigen::Vector3d(0.1, 1.2, 2.4);
  }

  Eigen::Vector3d origin() const { return worldFromCamera.topRightCorner<3, 1>(); }
};

// Corner walls x = -1.5 and z = -1.5, floor y = 0: nearest hit along o + t d.
bool hit(const Eigen::Vector3d& o, const Eigen::Vector3d& d, Eigen::Vector3d& p)
{
  double best = 1e9;
  if(d.z() < 0.)
    best = std::min(best, (-1.5 - o.z()) / d.z());
  if(d.x() < 0.)
    best = std::min(best, (-1.5 - o.x()) / d.x());
  if(d.y() < 0.)
    best = std::min(best, -o.y() / d.y());
  if(best > 1e8)
    return false;
  p = o + best * d;
  return true;
}

struct Bench
{
  procam::Model proj = projector();
  CameraSim cam;
  std::vector<Eigen::Vector3d> worldOfPixel;
  std::vector<uint8_t> hasWorld;
  std::vector<float> image, positions;

  Bench()
  {
    worldOfPixel.resize(CW * CH);
    hasWorld.assign(CW * CH, 0);
    positions.assign(CW * CH * 4, 0.f);
    image.assign(CW * CH * 4, 0.f);
    const Eigen::Matrix3d R = cam.worldFromCamera.topLeftCorner<3, 3>();
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        const Eigen::Vector3d ray((x - cam.cx) / cam.fx, (y - cam.cy) / cam.fy, 1.);
        Eigen::Vector3d p;
        if(!hit(cam.origin(), R * ray, p))
          continue;
        const int i = y * CW + x;
        worldOfPixel[i] = p;
        hasWorld[i] = 1;
        const Eigen::Vector3d pc = R.transpose() * (p - cam.origin());
        positions[i * 4 + 0] = float(pc.x());
        positions[i * 4 + 1] = float(pc.y());
        positions[i * 4 + 2] = float(pc.z());
        positions[i * 4 + 3] = 1.f;
      }
  }

  void render(const uint8_t* pattern, int pw, int ph)
  {
    for(int i = 0; i < CW * CH; i++)
    {
      float lum = 0.04f;
      Eigen::Vector2d px;
      if(pattern && hasWorld[i] && proj.project(worldOfPixel[i], px))
      {
        const int u = int(std::floor(px.x() + 0.5)), v = int(std::floor(px.y() + 0.5));
        if(u >= 0 && v >= 0 && u < pw && v < ph)
          lum += 0.8f * pattern[(std::size_t(v) * pw + u) * 4] / 255.f;
      }
      image[i * 4 + 0] = image[i * 4 + 1] = image[i * 4 + 2] = lum;
      image[i * 4 + 3] = 1.f;
    }
  }
};

void configure(StructuredLightCalibration& obj, Bench& b, int mode)
{
  obj.inputs.projector_size.value = {PW, PH};
  obj.inputs.mode.value = StructuredLightCalibration::Mode(mode);
  obj.inputs.world_from_camera.value = procam::toColumnMajor(b.cam.worldFromCamera);
  obj.inputs.intrinsics.value
      = {float(b.cam.fx), float(b.cam.fy), float(b.cam.cx), float(b.cam.cy)};
  obj.inputs.settle.value = 100;
  obj.inputs.frames.value = 2;
  obj.inputs.hole_radius.value = 4;
  obj.inputs.max_planes.value = 3;
}

double xyzRms(const StructuredLightCalibration& obj, int* count = nullptr)
{
  const auto gt = projector();
  const Eigen::Matrix4d W = gt.worldFromLocal();
  const auto& xyz = obj.outputs.xyz.texture;
  double se = 0.;
  int n = 0;
  for(int v = 0; v < PH; v += 3)
    for(int u = 0; u < PW; u += 3)
    {
      const float* p = xyz.bytes + (std::size_t(v) * PW + u) * 4;
      if(p[3] <= 0.f)
        continue;
      const Eigen::Vector3d dl((u - gt.cx) / gt.fx, (v - gt.cy) / gt.fy, 1.);
      Eigen::Vector3d truth;
      if(!hit(W.topRightCorner<3, 1>(), W.topLeftCorner<3, 3>() * dl, truth))
        continue;
      se += (Eigen::Vector3d(p[0], p[1], p[2]) - truth).squaredNorm();
      n++;
    }
  if(count)
    *count = n;
  return n > 0 ? std::sqrt(se / n) : 1e9;
}

//! Runs the sequence; returns the number of patterns shown.
int run(StructuredLightCalibration& obj, Bench& b)
{
  double now = 1000.;
  obj.tick(now);
  obj.inputs.start.value = halp::impulse_type{};
  int shown = 0;
  for(int i = 0; i < 20000; i++)
  {
    auto& pat = obj.outputs.pattern.texture;
    if(pat.changed)
      shown++;
    b.render(pat.bytes, pat.width, pat.height);
    pat.changed = false;
    obj.inputs.camera.texture.bytes = obj.inputs.camera.request_readback ? b.image.data() : nullptr;
    obj.inputs.camera.texture.width = CW;
    obj.inputs.camera.texture.height = CH;
    obj.inputs.positions.texture.bytes
        = obj.inputs.positions.request_readback ? b.positions.data() : nullptr;
    obj.inputs.positions.texture.width = CW;
    obj.inputs.positions.texture.height = CH;
    now += 16.7;
    obj.tick(now);
    obj.inputs.start.value.reset();
    if(obj.outputs.done.value)
      return shown;
    if(!obj.running())
      return shown;
    if(obj.outputs.status.value == "Decoding")
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      i--;
    }
  }
  return shown;
}
}

TEST_CASE("Gray sequence follows CONVENTIONS and skips thin stripes", "[threedim][structuredlight]")
{
  const auto s = sl::GraySequence::make(1920, 1080, 1);
  CHECK(s.bitsX == 11);
  CHECK(s.bitsY == 11);
  CHECK(s.count() == 2 + 2 * 22);
  CHECK(s.lit(0, 5, 5));
  CHECK_FALSE(s.lit(1, 5, 5));
  for(int u : {0, 1, 700, 1919})
  {
    const int g = u ^ (u >> 1);
    CHECK(s.lit(2, u, 0) == bool((g >> 10) & 1));
    CHECK(s.lit(3, u, 0) != s.lit(2, u, 0));
    CHECK(s.lit(2 + 2 * 10, u, 0) == bool(g & 1));
  }
  const int v = 777, g = v ^ (v >> 1);
  CHECK(s.lit(2 + 2 * 11, 0, v) == bool((g >> 10) & 1));

  const auto t = sl::GraySequence::make(1920, 1080, 8);
  CHECK(t.skip == 2);
  CHECK(t.count() == 2 + 2 * (9 + 9));
}

TEST_CASE("Structured light, depth camera: XYZ map and projector pose", "[threedim][structuredlight]")
{
  Bench b;
  StructuredLightCalibration obj;
  configure(obj, b, StructuredLightCalibration::DepthCamera);
  const int shown = run(obj, b);
  INFO(obj.outputs.status.value);
  REQUIRE(obj.outputs.done.value);
  CHECK(shown >= sl::GraySequence::make(PW, PH, 2).count());
  CHECK(obj.outputs.coverage.value > 30.f);
  CHECK(obj.outputs.progress.value == Approx(0.99f).margin(0.02f));

  const auto& k = obj.outputs.intrinsics.value;
  const auto gt = projector();
  CHECK(k[0] == Approx(gt.fx).epsilon(0.01));
  CHECK(k[1] == Approx(gt.fy).epsilon(0.01));
  CHECK(k[2] == Approx(gt.cx).margin(4.));
  CHECK(k[3] == Approx(gt.cy).margin(4.));
  CHECK(obj.outputs.rms.value < 1.f);
  const auto M = procam::fromColumnMajor(obj.outputs.pose.value);
  CHECK((M.topRightCorner<3, 1>() - gt.position()).norm() < 0.02);
  CHECK(obj.outputs.m3.value[0] == obj.outputs.pose.value[12]);

  // Planes (the default): the fitted corner, ray-cast through the solved model.
  int n = 0;
  const double planesRms = xyzRms(obj, &n);
  REQUIRE(n > 1000);
  CHECK(planesRms < 0.005);
  const auto& xyz = obj.outputs.xyz.texture;
  REQUIRE(xyz.bytes);
  REQUIRE(xyz.width == PW);
  const auto& planes = obj.outputs.planes.value;
  CHECK(planes.size() == 3);
  for(const auto& pl : planes)
  {
    const Eigen::Vector3d nrm(pl[0], pl[1], pl[2]);
    CHECK(std::abs(nrm.norm() - 1.) < 1e-3);
    const bool wallX = std::abs(std::abs(nrm.x()) - 1.) < 0.01 && std::abs(std::abs(pl[3]) - 1.5) < 0.01;
    const bool wallZ = std::abs(std::abs(nrm.z()) - 1.) < 0.01 && std::abs(std::abs(pl[3]) - 1.5) < 0.01;
    const bool floor = std::abs(std::abs(nrm.y()) - 1.) < 0.01 && std::abs(pl[3]) < 0.01;
    CHECK((wallX || wallZ || floor));
  }

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string path = dir.filePath("proj0").toStdString();
  StructuredLightCalibration::Result r;
  r.ok = true;
  r.width = PW;
  r.height = PH;
  r.xyz.assign(xyz.bytes, xyz.bytes + std::size_t(PW) * PH * 4);
  r.intrinsics = k;
  r.pose = obj.outputs.pose.value;
  r.rms = obj.outputs.rms.value;
  r.planes = planes;
  REQUIRE(StructuredLightCalibration::saveFile(path, r));
  const auto back = StructuredLightCalibration::loadFile(path + ".xyz.pfm");
  REQUIRE(back.ok);
  CHECK(back.xyz == r.xyz);
  CHECK(back.intrinsics == r.intrinsics);
  CHECK(back.pose == r.pose);
  CHECK(back.planes == r.planes);
  CHECK(back.coverage == Approx(obj.outputs.coverage.value).margin(1e-3));

  obj.inputs.save_path.value = dir.filePath("again.pfm").toStdString();
  obj.inputs.save.value = halp::impulse_type{};
  obj.tick(1e7);
  CHECK(QFile::exists(dir.filePath("again.xyz.pfm")));
  CHECK(QFile::exists(dir.filePath("again.valid.pfm")));
  CHECK(QFile::exists(dir.filePath("again.json")));
}

TEST_CASE("Structured light, depth camera: surface models", "[threedim][structuredlight]")
{
  double rms[3]{};
  int planes[3]{};
  for(int surface : {StructuredLightCalibration::Planes, StructuredLightCalibration::SmoothMeasured,
                     StructuredLightCalibration::RawMeasured})
  {
    Bench b;
    StructuredLightCalibration obj;
    configure(obj, b, StructuredLightCalibration::DepthCamera);
    obj.inputs.surface.value = StructuredLightCalibration::Surface(surface);
    obj.inputs.max_planes.value = 3;
    run(obj, b);
    INFO(obj.outputs.status.value);
    REQUIRE(obj.outputs.done.value);
    rms[surface] = xyzRms(obj);
    planes[surface] = int(obj.outputs.planes.value.size());
  }
  CHECK(rms[StructuredLightCalibration::Planes] < 0.005);
  CHECK(rms[StructuredLightCalibration::SmoothMeasured] < 0.01);
  CHECK(rms[StructuredLightCalibration::RawMeasured] < 0.03);
  CHECK(rms[StructuredLightCalibration::Planes] < rms[StructuredLightCalibration::RawMeasured]);
  CHECK(rms[StructuredLightCalibration::SmoothMeasured] < rms[StructuredLightCalibration::RawMeasured]);
  CHECK(planes[StructuredLightCalibration::Planes] == 3);
  CHECK(planes[StructuredLightCalibration::RawMeasured] == 0);
}

TEST_CASE("Structured light: one driver per process instance", "[threedim][structuredlight]")
{
  Bench b;
  StructuredLightCalibration a, second;
  configure(a, b, StructuredLightCalibration::DepthCamera);
  configure(second, b, StructuredLightCalibration::DepthCamera);
  a.prepare({.instance = 4242});
  second.prepare({.instance = 4242});

  double now = 1000.;
  a.tick(now);
  second.tick(now);
  a.inputs.start.value = halp::impulse_type{};
  second.inputs.start.value = halp::impulse_type{};
  bool mirrored = true;
  for(int i = 0; i < 20000 && !a.outputs.done.value; i++)
  {
    auto& pat = a.outputs.pattern.texture;
    b.render(pat.bytes, pat.width, pat.height);
    a.inputs.camera.texture = {b.image.data(), CW, CH, true};
    a.inputs.positions.texture = {b.positions.data(), CW, CH, true};
    second.inputs.camera.texture = {b.image.data(), CW, CH, true};
    second.inputs.positions.texture = {b.positions.data(), CW, CH, true};
    now += 16.7;
    a.tick(now);
    second.tick(now);
    a.inputs.start.value.reset();
    second.inputs.start.value.reset();
    const auto& p2 = second.outputs.pattern.texture;
    if(second.running() || p2.width != pat.width
       || std::memcmp(p2.bytes, pat.bytes, std::size_t(pat.width) * pat.height * 4) != 0
       || second.outputs.status.value != a.outputs.status.value)
      mirrored = false;
    if(a.outputs.status.value == "Decoding")
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      i--;
    }
  }
  REQUIRE(a.outputs.done.value);
  CHECK(mirrored);
  second.tick(now + 20.);
  CHECK(second.outputs.coverage.value == a.outputs.coverage.value);
  CHECK(second.outputs.intrinsics.value == a.outputs.intrinsics.value);
}

TEST_CASE("Structured light, viewer camera: direction map", "[threedim][structuredlight]")
{
  Bench b;
  StructuredLightCalibration obj;
  configure(obj, b, StructuredLightCalibration::ViewerCamera);
  run(obj, b);
  INFO(obj.outputs.status.value);
  REQUIRE(obj.outputs.done.value);
  CHECK(obj.outputs.coverage.value > 30.f);

  const auto gt = projector();
  const Eigen::Matrix4d W = gt.worldFromLocal();
  const auto& xyz = obj.outputs.xyz.texture;
  REQUIRE(xyz.bytes);
  double worst = 0., sum = 0.;
  int n = 0;
  for(int v = 0; v < PH; v += 4)
    for(int u = 0; u < PW; u += 4)
    {
      const float* p = xyz.bytes + (std::size_t(v) * PW + u) * 4;
      if(p[3] <= 0.f)
        continue;
      const Eigen::Vector3d dl((u - gt.cx) / gt.fx, (v - gt.cy) / gt.fy, 1.);
      Eigen::Vector3d truth;
      if(!hit(W.topRightCorner<3, 1>(), W.topLeftCorner<3, 3>() * dl, truth))
        continue;
      const Eigen::Vector3d d(p[0], p[1], p[2]);
      CHECK(std::abs(d.norm() - 1.) < 1e-3);
      const double a
          = std::acos(std::clamp(d.dot((truth - b.cam.origin()).normalized()), -1., 1.));
      worst = std::max(worst, a);
      sum += a;
      n++;
    }
  REQUIRE(n > 1000);
  CHECK(sum / n < 0.5 * M_PI / 180.);
}

TEST_CASE("Structured light: abort and the idle pattern", "[threedim][structuredlight]")
{
  StructuredLightCalibration obj;
  obj.inputs.projector_size.value = {64, 32};
  obj.inputs.idle.value = StructuredLightCalibration::TestGrid;
  obj.tick(0.);
  REQUIRE(obj.outputs.pattern.texture.bytes);
  CHECK(obj.outputs.pattern.texture.width == 64);
  CHECK(obj.outputs.pattern.texture.bytes[0] == 255);
  CHECK_FALSE(obj.inputs.camera.request_readback);

  obj.inputs.start.value = halp::impulse_type{};
  obj.tick(10.);
  obj.inputs.start.value.reset();
  CHECK(obj.running());
  CHECK(obj.inputs.camera.request_readback);
  obj.inputs.abort.value = halp::impulse_type{};
  obj.tick(20.);
  CHECK_FALSE(obj.running());
  CHECK_FALSE(obj.inputs.camera.request_readback);
  CHECK(obj.outputs.status.value == "Aborted");
}

TEST_CASE("Structured light: intrinsics modes", "[threedim][structuredlight][intrinsics]")
{
  Bench b;
  StructuredLightCalibration obj;
  configure(obj, b, StructuredLightCalibration::DepthCamera);
  obj.inputs.accumulate_intrinsics.value = true;
  run(obj, b);
  REQUIRE(obj.outputs.done.value);
  const auto solved = obj.outputs.intrinsics.value;
  run(obj, b);
  REQUIRE(obj.outputs.done.value);
  const auto& acc = obj.outputs.accumulated_intrinsics.value;
  CHECK(acc[0] == Approx(solved[0]).epsilon(0.01));
  CHECK(acc[2] == Approx(solved[2]).margin(2.));

  obj.inputs.intrinsics_mode.value = StructuredLightCalibration::LockToLoaded;
  run(obj, b);
  INFO(obj.outputs.status.value);
  REQUIRE(obj.outputs.done.value);
  CHECK(obj.outputs.status.value.find("intrinsics locked") != std::string::npos);
  CHECK(obj.outputs.intrinsics.value[0] == Approx(solved[0]).epsilon(0.01));
  const auto M = procam::fromColumnMajor(obj.outputs.pose.value);
  CHECK((M.topRightCorner<3, 1>() - projector().position()).norm() < 0.02);

  StructuredLightCalibration lock;
  configure(lock, b, StructuredLightCalibration::DepthCamera);
  lock.inputs.intrinsics_mode.value = StructuredLightCalibration::LockIntrinsics;
  run(lock, b);
  REQUIRE(lock.outputs.done.value);
  CHECK(lock.outputs.status.value.find("solved instead") != std::string::npos);

  lock.inputs.throw_ratio.value = 1.1f;
  lock.inputs.lens_shift.value = {0.f, 0.f};
  run(lock, b);
  REQUIRE(lock.outputs.done.value);
  CHECK(lock.outputs.intrinsics.value[0] == Approx(1.1f * PW));
  CHECK(lock.outputs.intrinsics.value[2] == Approx((PW - 1) / 2.f));
  CHECK(lock.outputs.status.value.find("intrinsics locked") != std::string::npos);
}
