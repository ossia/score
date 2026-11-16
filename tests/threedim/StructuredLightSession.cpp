#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/StructuredLightSession.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

using Threedim::StructuredLightSession;
namespace procam = score::gfx::procam;

namespace
{
constexpr int PW = 160, PH = 100;
constexpr int CW = 480, CH = 200;
constexpr int Projectors = 6;

procam::Model projector(double x)
{
  procam::Model m;
  m.fx = m.fy = 200.;
  m.cx = 79.5;
  m.cy = 49.5;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  m.R = (Eigen::AngleAxisd(0.05 * x, Eigen::Vector3d::UnitY()).toRotationMatrix() * flip)
            .transpose();
  m.t = -m.R * Eigen::Vector3d(x, 1.2, 0.);
  return m;
}

struct Bench
{
  std::vector<procam::Model> proj;
  Eigen::Matrix4d worldFromCamera = Eigen::Matrix4d::Identity();
  double fx = 110., fy = 110., cx = 239.5, cy = 99.5;
  std::vector<Eigen::Vector3d> world;
  std::vector<uint8_t> has;
  std::vector<float> image, positions;

  Bench()
  {
    for(int i = 0; i < Projectors; i++)
      proj.push_back(projector(-1.5 + 0.6 * i));
    const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
    worldFromCamera.topLeftCorner<3, 3>() = flip;
    worldFromCamera.topRightCorner<3, 1>() = Eigen::Vector3d(0., 1.2, 0.3);
    world.resize(CW * CH);
    has.assign(CW * CH, 0);
    image.assign(CW * CH * 4, 0.f);
    positions.assign(CW * CH * 4, 0.f);
    const Eigen::Vector3d o = worldFromCamera.topRightCorner<3, 1>();
    for(int y = 0; y < CH; y++)
      for(int x = 0; x < CW; x++)
      {
        const Eigen::Vector3d d = flip * Eigen::Vector3d((x - cx) / fx, (y - cy) / fy, 1.);
        if(d.z() >= 0.)
          continue;
        const Eigen::Vector3d p = o + ((-2. - o.z()) / d.z()) * d;
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

  void render(const StructuredLightSession::Display& d)
  {
    for(int i = 0; i < CW * CH; i++)
    {
      float lin = 0.02f;
      for(int j = 0; j < Projectors && has[i]; j++)
      {
        if(!d.idle && j != d.projector)
          continue;
        Eigen::Vector2d px;
        if(d.rgba.empty() || !proj[j].project(world[i], px))
          continue;
        const int u = int(std::floor(px.x() + 0.5)), v = int(std::floor(px.y() + 0.5));
        if(u < 0 || v < 0 || u >= d.width || v >= d.height)
          continue;
        lin += 0.8f * d.rgba[(std::size_t(v) * d.width + u) * 4] / 255.f;
      }
      const float c = std::min(1.f, lin);
      image[i * 4 + 0] = image[i * 4 + 1] = image[i * 4 + 2] = c;
      image[i * 4 + 3] = 1.f;
    }
  }
};
}

TEST_CASE(
    "StructuredLightSession calibrates six projectors in one run",
    "[threedim][structuredlight]")
{
  Bench b;
  StructuredLightSession session;
  StructuredLightSession::Settings s;
  s.projectorWidth = PW;
  s.projectorHeight = PH;
  s.projectors = Projectors;
  s.mode = StructuredLightSession::DepthCamera;
  s.worldFromCamera = procam::toColumnMajor(b.worldFromCamera);
  s.cameraIntrinsics = {float(b.fx), float(b.fy), float(b.cx), float(b.cy)};
  s.settleMs = 40;
  s.frames = 1;
  s.holeRadius = 4;
  s.surface = StructuredLightSession::RawMeasured;

  double now = 1000.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, now, ev));
  REQUIRE_FALSE(session.display().idle);
  CHECK(session.display().projector == 0);

  std::vector<int> projectorsSeen;
  bool canvasChecked = false;
  bool done = false;
  for(int i = 0; i < 200000 && !done; i++)
  {
    const auto& d = session.display();
    if(!d.idle
       && (projectorsSeen.empty() || projectorsSeen.back() != d.projector))
      projectorsSeen.push_back(d.projector);
    if(!d.idle && d.projector == 5 && !canvasChecked)
    {
      constexpr int W = 600, H = 100;
      std::vector<uint8_t> canvas(W * H * 4);
      session.renderCanvas(W, H, "", Projectors, canvas.data());
      auto lit = [&](int p) {
        int n = 0;
        for(int y = 0; y < H; y++)
          for(int x = p * 100; x < (p + 1) * 100; x++)
            n += canvas[(y * W + x) * 4] > 0;
        return n;
      };
      for(int p = 0; p < 5; p++)
        CHECK(lit(p) == 0);
      CHECK(lit(5) > 0);
      canvasChecked = true;
    }
    b.render(d);
    StructuredLightSession::Frame f;
    if(session.wantsCamera())
    {
      f.rgba = b.image.data();
      f.width = CW;
      f.height = CH;
    }
    if(session.wantsPositions())
    {
      f.positions = b.positions.data();
      f.positionsWidth = CW;
      f.positionsHeight = CH;
    }
    now += 16.7;
    ev = {};
    session.tick(s, now, f, ev);
    done = ev.done;
    if(!done && !session.running())
      FAIL(session.status());
    if(session.status() == "Decoding")
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  INFO(session.status());
  REQUIRE(done);
  CHECK(canvasChecked);
  CHECK(projectorsSeen == std::vector<int>{0, 1, 2, 3, 4, 5});
  CHECK(session.display().idle);

  const auto& results = session.results();
  REQUIRE(results.size() == Projectors);
  for(int p = 0; p < Projectors; p++)
  {
    const auto& r = *results[p];
    INFO("projector " << p << ": " << r.status);
    CHECK(r.ok);
    CHECK(r.projectorIndex == p);
    CHECK(r.coverage > 50.f);
    std::vector<float> err;
    for(std::size_t i = 0; i + 3 < r.xyz.size(); i += 4)
      if(r.xyz[i + 3] > 0.5f)
        err.push_back(std::abs(r.xyz[i + 2] + 2.f));
    REQUIRE(!err.empty());
    std::nth_element(err.begin(), err.begin() + err.size() / 2, err.end());
    CHECK(err[err.size() / 2] < 0.01f);
  }
}
