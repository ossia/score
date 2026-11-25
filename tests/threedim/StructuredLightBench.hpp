#pragma once
// A simulated structured light bench: projectors on a wall at z = -2, a depth
// camera seeing them, with a latency, dropped and frozen frames, a camera
// gamma, a gain that clips; and a bounded run loop.
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/StructuredLightSession.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <random>
#include <vector>

namespace slbench
{
using Threedim::StructuredLightSession;
namespace procam = score::gfx::procam;

constexpr int PW = 160, PH = 100;
constexpr int CW = 480, CH = 200;

inline procam::Model projector(double x)
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
  //! Projectors the camera does not see.
  std::vector<int> hidden;
  Eigen::Matrix4d worldFromCamera = Eigen::Matrix4d::Identity();
  double fx = 110., fy = 110., cx = 239.5, cy = 99.5;
  std::vector<Eigen::Vector3d> world;
  std::vector<uint8_t> has;
  std::vector<float> positions;

  float ambient{0.02f};
  float gain{0.8f};
  //! Camera code = light^(1 / encodeGamma).
  float encodeGamma{1.f};
  //! The projector's emitted light = code^projectorGamma.
  float projectorGamma{1.f};
  double latencyMs{};
  //! Every n-th frame is missing; 0 = none.
  int dropEvery{};
  //! After this time the camera repeats its last frame; < 0 = never.
  double freezeAt{-1.};
  //! After this time the camera delivers nothing; < 0 = never.
  double stopAt{-1.};
  float noise{};
  uint64_t serial{};

  std::deque<std::pair<double, std::vector<float>>> history;
  std::vector<float> frame, rendered;
  std::vector<int> shownVersions;
  std::mt19937 rng{3};
  int ticks{};

  explicit Bench(int projectors)
  {
    for(int i = 0; i < projectors; i++)
      proj.push_back(projector(projectors == 1 ? 0. : -1.5 + 3. * i / (projectors - 1)));
    const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
    worldFromCamera.topLeftCorner<3, 3>() = flip;
    worldFromCamera.topRightCorner<3, 1>() = Eigen::Vector3d(0., 1.2, 0.3);
    world.resize(CW * CH);
    has.assign(CW * CH, 0);
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

  //! Per projector, the projector pixel each camera pixel sees, -1 when none.
  std::vector<std::vector<int>> seen;

  void map()
  {
    seen.assign(proj.size(), std::vector<int>(std::size_t(CW) * CH, -1));
    for(std::size_t j = 0; j < proj.size(); j++)
      for(int i = 0; i < CW * CH; i++)
      {
        Eigen::Vector2d px;
        if(!has[i] || !proj[j].project(world[i], px))
          continue;
        const int u = int(std::floor(px.x() + 0.5)), v = int(std::floor(px.y() + 0.5));
        if(u >= 0 && v >= 0 && u < PW && v < PH)
          seen[j][i] = v * PW + u;
      }
  }

  std::vector<float> render(const StructuredLightSession& s)
  {
    if(seen.size() != proj.size())
      map();
    std::vector<float> image(std::size_t(CW) * CH * 4, 0.f);
    std::normal_distribution<float> n(0.f, std::max(noise, 1e-9f));
    for(int i = 0; i < CW * CH; i++)
    {
      float lin = ambient;
      for(int j = 0; j < int(proj.size()); j++)
      {
        if(std::find(hidden.begin(), hidden.end(), j) != hidden.end())
          continue;
        const auto& d = s.image(j);
        const int q = seen[j][i];
        if(q < 0 || d.rgba.empty() || d.width != PW || d.height != PH)
          continue;
        const float code = d.rgba[std::size_t(q) * 4] / 255.f;
        lin += gain * std::pow(code, projectorGamma);
      }
      if(noise > 0.f)
        lin += n(rng);
      const float c = std::clamp(std::pow(std::max(lin, 0.f), 1.f / encodeGamma), 0.f, 1.f);
      image[i * 4 + 0] = image[i * 4 + 1] = image[i * 4 + 2] = c;
      image[i * 4 + 3] = 1.f;
    }
    return image;
  }

  //! The frame the camera delivers at `now`, or null.
  const float* capture(const StructuredLightSession& s, double now)
  {
    ticks++;
    if(stopAt >= 0. && now >= stopAt)
      return nullptr;
    if(!(freezeAt >= 0. && now >= freezeAt && !frame.empty()))
    {
      std::vector<int> versions;
      for(int j = 0; j < s.images(); j++)
        versions.push_back(s.image(j).version);
      if(versions != shownVersions || history.empty() || noise > 0.f)
      {
        rendered = render(s);
        shownVersions = versions;
      }
      history.emplace_back(now, rendered);
      while(history.size() > 1 && history[1].first <= now - latencyMs)
        history.pop_front();
      frame = history.front().second;
      serial++;
    }
    if(dropEvery > 0 && ticks % dropEvery == 0)
      return nullptr;
    return frame.data();
  }

  StructuredLightSession::Settings settings() const
  {
    StructuredLightSession::Settings s;
    s.projectorWidth = PW;
    s.projectorHeight = PH;
    s.projectors = int(proj.size());
    s.mode = StructuredLightSession::DepthCamera;
    s.worldFromCamera = procam::toColumnMajor(worldFromCamera);
    s.cameraIntrinsics = {float(fx), float(fy), float(cx), float(cy)};
    s.settleMs = 40;
    s.frames = 1;
    s.holeRadius = 4;
    s.timeoutMs = 1000;
    s.surface = StructuredLightSession::RawMeasured;
    return s;
  }
};

struct RunLog
{
  bool done{};
  std::vector<StructuredLightSession::Events> events;
  double endedAt{};
  int ticks{};
};

//! Ticks every 16.7 ms until the run ends or `maxTicks`; `each` runs before
//! every tick.
inline RunLog run(
    StructuredLightSession& session, const StructuredLightSession::Settings& s, Bench& b,
    double& now, int maxTicks = 20000,
    const std::function<void(int)>& each = {}, bool untilDecoding = false)
{
  RunLog log;
  for(int i = 0; i < maxTicks && session.running(); i++)
  {
    if(untilDecoding && session.status() == "Decoding")
      break;
    if(each)
      each(i);
    StructuredLightSession::Frame f;
    const float* img = b.capture(session, now);
    if(session.wantsCamera() && img)
      f.camera = {img, CW, CH};
    if(session.wantsPositions())
    {
      f.positions = b.positions.data();
      f.positionsWidth = CW;
      f.positionsHeight = CH;
    }
    now += 16.7;
    StructuredLightSession::Events ev;
    session.tick(s, now, f, ev);
    if(ev.exposure || ev.autoExposure || ev.done)
      log.events.push_back(ev);
    log.done |= ev.done;
    log.ticks = i;
  }
  log.endedAt = now;
  return log;
}

inline float medianDepthError(const StructuredLightSession::Result& r)
{
  std::vector<float> err;
  for(std::size_t i = 0; i + 3 < r.xyz.size(); i += 4)
    if(r.xyz[i + 3] > 0.5f)
      err.push_back(std::abs(r.xyz[i + 2] + 2.f));
  if(err.empty())
    return 1e9f;
  std::nth_element(err.begin(), err.begin() + err.size() / 2, err.end());
  return err[err.size() / 2];
}
}
