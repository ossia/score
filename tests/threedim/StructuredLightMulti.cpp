// Structured Light Calibration with two projectors in one run (each on its own
// pattern outlet) and the photometric maps: a simulated RGB camera with a
// gamma-encoded response, two projectors of different brightness, tint and
// vignetting, and ambient light.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/CalibrationFiles.hpp>
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

namespace
{
constexpr int PW = 320, PH = 200;
constexpr int CW = 400, CH = 240;
constexpr float camGamma = 2.2f;
constexpr float ambient[3] = {0.02f, 0.02f, 0.03f};

procam::Model projector(double x)
{
  procam::Model m;
  m.fx = m.fy = 300.;
  m.cx = 159.5;
  m.cy = 99.5;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  m.R = (Eigen::AngleAxisd(0.05 * x, Eigen::Vector3d::UnitY()).toRotationMatrix() * flip)
            .transpose();
  m.t = -m.R * Eigen::Vector3d(x, 1.2, 0.);
  return m;
}

// Wall z = -2 with a floor bump, so the solve is not planar.
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

struct Projector
{
  procam::Model model;
  float gain[3];
  double vignette;
  float white(int c, double u, double v) const
  {
    const double dx = (u - model.cx) / PW, dy = (v - model.cy) / PH;
    return float(gain[c] * (1. - vignette * (dx * dx + dy * dy)));
  }
};

struct Bench
{
  std::vector<Projector> proj{
      {projector(-0.6), {0.9f, 0.8f, 0.7f}, 0.8},
      {projector(0.6), {0.6f, 0.6f, 0.65f}, 0.4}};
  Eigen::Matrix4d worldFromCamera = Eigen::Matrix4d::Identity();
  double fx = 260., fy = 260., cx = 199.5, cy = 119.5;
  std::vector<Eigen::Vector3d> world;
  std::vector<uint8_t> has;
  std::vector<float> image, positions;

  Bench()
  {
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

  void render(const std::vector<const uint8_t*>& patterns, int pw, int ph)
  {
    for(int i = 0; i < CW * CH; i++)
    {
      float lin[3] = {ambient[0], ambient[1], ambient[2]};
      for(std::size_t j = 0; j < proj.size() && has[i]; j++)
      {
        Eigen::Vector2d px;
        if(!patterns[j] || !proj[j].model.project(world[i], px))
          continue;
        const int u = int(std::floor(px.x() + 0.5)), v = int(std::floor(px.y() + 0.5));
        if(u < 0 || v < 0 || u >= pw || v >= ph)
          continue;
        const uint8_t* q = patterns[j] + (std::size_t(v) * pw + u) * 4;
        for(int c = 0; c < 3; c++)
          lin[c] += proj[j].white(c, u, v) * q[c] / 255.f;
      }
      for(int c = 0; c < 3; c++)
        image[i * 4 + c] = std::pow(std::min(1.f, lin[c]), 1.f / camGamma);
      image[i * 4 + 3] = 1.f;
    }
  }
};

bool run(StructuredLightCalibration& obj, Bench& b)
{
  double now = 1000.;
  obj.tick(now);
  obj.inputs.start.value = halp::impulse_type{};
  for(int i = 0; i < 40000; i++)
  {
    auto& p1 = obj.outputs.pattern.texture;
    auto& p2 = obj.outputs.pattern2.texture;
    b.render({p1.bytes, p2.bytes}, p1.width, p1.height);
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
    if(obj.outputs.done.value)
      return true;
    if(!obj.running())
      return false;
    if(obj.outputs.status.value == "Decoding")
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      i--;
    }
  }
  return false;
}

double xyzError(const float* xyz, const procam::Model& m, int* count)
{
  const Eigen::Matrix4d W = m.worldFromLocal();
  double se = 0.;
  int n = 0;
  for(int v = 0; v < PH; v += 3)
    for(int u = 0; u < PW; u += 3)
    {
      const float* p = xyz + (std::size_t(v) * PW + u) * 4;
      if(p[3] <= 0.f)
        continue;
      Eigen::Vector3d t;
      if(!hit(
             W.topRightCorner<3, 1>(),
             W.topLeftCorner<3, 3>() * Eigen::Vector3d((u - m.cx) / m.fx, (v - m.cy) / m.fy, 1.),
             t))
        continue;
      se += (Eigen::Vector3d(p[0], p[1], p[2]) - t).squaredNorm();
      n++;
    }
  *count = n;
  return n ? std::sqrt(se / n) : 1e9;
}
}

TEST_CASE("Structured light: two projectors in one run, with photometric maps", "[threedim][structuredlight]")
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
  obj.inputs.camera_gamma.value = camGamma;
  obj.inputs.projector_count.value = 2;

  REQUIRE(run(obj, b));
  INFO(obj.outputs.status.value);

  int n1 = 0, n2 = 0;
  const double e1 = xyzError(obj.outputs.xyz.texture.bytes, b.proj[0].model, &n1);
  const double e2 = xyzError(obj.outputs.xyz2.texture.bytes, b.proj[1].model, &n2);
  INFO("XYZ RMS projector 1 " << e1 << " m on " << n1 << ", projector 2 " << e2 << " m on " << n2);
  CHECK(n1 > 1000);
  CHECK(n2 > 1000);
  CHECK(e1 < 0.01);
  CHECK(e2 < 0.01);

  const auto& lum = obj.outputs.lum.texture;
  const auto& blk = obj.outputs.black.texture;
  REQUIRE(lum.width == PW);
  REQUIRE(blk.width == PW);
  double relErr = 0., blackErr = 0.;
  int m = 0;
  for(int v = 20; v < PH - 20; v += 5)
    for(int u = 20; u < PW - 20; u += 5)
    {
      const float* l = lum.bytes + (v * PW + u) * 4;
      const float* k = blk.bytes + (v * PW + u) * 4;
      if(l[3] <= 0.f)
        continue;
      for(int c = 0; c < 3; c++)
      {
        relErr = std::max(relErr, std::abs(l[c] / b.proj[0].white(c, u, v) - 1.));
        blackErr = std::max(blackErr, double(std::abs(k[c] - ambient[c])));
      }
      m++;
    }
  INFO("white - black within " << relErr * 100. << " %, black within " << blackErr << " on "
                               << m << " pixels");
  CHECK(m > 500);
  CHECK(relErr < 0.05);
  CHECK(blackErr < 0.005);

  QTemporaryDir dir;
  obj.inputs.save_path.value = dir.filePath("room").toStdString();
  obj.inputs.save.value = halp::impulse_type{};
  obj.tick(1e9);
  obj.inputs.save.value.reset();
  INFO(obj.outputs.status.value);
  const auto c1 = Threedim::loadCalibration(dir.filePath("room.xyz.pfm").toStdString());
  const auto c2 = Threedim::loadCalibration(dir.filePath("room_p1.json").toStdString());
  REQUIRE(c1.ok());
  REQUIRE(c2.ok());
  CHECK(c1.lum.size() == std::size_t(PW * PH * 3));
  CHECK(c2.black.size() == std::size_t(PW * PH * 3));
  CHECK(c2.hasModel());
  REQUIRE(QFile::copy(dir.filePath("room_p1.json"), dir.filePath("alias.json")));
  const auto c3 = Threedim::loadCalibration(dir.filePath("alias.json").toStdString());
  REQUIRE(c3.ok());
  CHECK(c3.black.size() == std::size_t(PW * PH * 3));
  CHECK(c3.hasModel());
  const auto M2 = procam::fromColumnMajor(c2.pose);
  CHECK((M2.topRightCorner<3, 1>() - b.proj[1].model.position()).norm() < 0.03);
}
