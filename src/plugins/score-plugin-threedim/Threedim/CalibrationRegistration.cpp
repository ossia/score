#include "CalibrationRegistration.hpp"

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <chrono>
#include <cstdio>

namespace Threedim
{
namespace reg = score::gfx::registration;
namespace procam = score::gfx::procam;

RegisterCalibrations::Result RegisterCalibrations::solve(
    const CalibrationFile& a, const CalibrationFile& b, double threshold, bool allowScale,
    int step)
{
  Result r;
  if(!a.ok() || !b.ok())
  {
    r.status = "Failed: " + (a.ok() ? b.error : a.error);
    return r;
  }
  if(a.width != b.width || a.height != b.height)
  {
    r.status = "Failed: the two calibrations of the shared projector differ in size";
    return r;
  }
  if(a.viewerMode || b.viewerMode)
  {
    r.status = "Failed: viewer-camera direction maps carry no position to register";
    return r;
  }
  step = std::max(1, step);
  std::vector<reg::Vec3> x, y;
  for(int v = 0; v < a.height; v += step)
    for(int u = 0; u < a.width; u += step)
    {
      const std::size_t i = (std::size_t(v) * a.width + u) * 4;
      if(a.xyz[i + 3] <= 0.f || b.xyz[i + 3] <= 0.f)
        continue;
      x.emplace_back(b.xyz[i], b.xyz[i + 1], b.xyz[i + 2]);
      y.emplace_back(a.xyz[i], a.xyz[i + 1], a.xyz[i + 2]);
    }
  r.correspondences = int(x.size());
  const auto res = reg::ransac(x, y, threshold, allowScale);
  if(!res.ok)
  {
    r.status = "Failed: " + res.error + " (" + std::to_string(x.size())
               + " pixels lit in both)";
    return r;
  }
  r.transform = procam::toColumnMajor(res.transform.matrix());
  r.rms = res.rms;
  r.scale = res.transform.scale;
  r.inliers = res.inliers;
  r.ok = true;
  char buf[200];
  std::snprintf(
      buf, sizeof(buf), "Registered: %d / %d pixels within %.3f m, RMS %.4f m, scale %.4f",
      res.inliers, int(x.size()), threshold, res.rms, res.transform.scale);
  r.status = buf;
  return r;
}

void RegisterCalibrations::apply(CalibrationFile& c, const std::array<float, 16>& transform)
{
  const Eigen::Matrix4d T = procam::fromColumnMajor(transform);
  const Eigen::Matrix3d sR = T.topLeftCorner<3, 3>();
  const double s = std::cbrt(std::max(1e-300, sR.determinant()));
  const Eigen::Matrix3d R = sR / s;
  const Eigen::Vector3d t = T.topRightCorner<3, 1>();
  for(std::size_t i = 0; i + 3 < c.xyz.size(); i += 4)
  {
    if(c.xyz[i + 3] <= 0.f)
      continue;
    const Eigen::Vector3d p(c.xyz[i], c.xyz[i + 1], c.xyz[i + 2]);
    const Eigen::Vector3d q = c.viewerMode ? Eigen::Vector3d(R * p) : Eigen::Vector3d(sR * p + t);
    c.xyz[i] = float(q.x());
    c.xyz[i + 1] = float(q.y());
    c.xyz[i + 2] = float(q.z());
  }
  if(c.hasPose)
  {
    Eigen::Matrix4d P = procam::fromColumnMajor(c.pose);
    Eigen::Matrix4d Q = Eigen::Matrix4d::Identity();
    Q.topLeftCorner<3, 3>() = R * P.topLeftCorner<3, 3>();
    Q.topRightCorner<3, 1>() = sR * P.topRightCorner<3, 1>() + t;
    c.pose = procam::toColumnMajor(Q);
  }
}

RegisterCalibrations::Outcome RegisterCalibrations::run(const Job& job)
{
  Outcome o;
  const auto a = loadCalibration(job.reference);
  const auto b = loadCalibration(job.other);
  o.result = solve(a, b, job.threshold, job.allowScale, job.step);
  o.status = o.result.status;
  if(!o.result.ok)
    return o;
  int failed = 0;
  for(const auto& path : splitPaths(job.move))
  {
    auto c = loadCalibration(path);
    if(!c.ok())
    {
      failed++;
      continue;
    }
    const std::string src = c.base;
    apply(c, o.result.transform);
    c.base = src + job.suffix;
    if(c.base == src || !saveCalibration(c, src))
    {
      failed++;
      continue;
    }
    o.written.push_back(c.base + ".xyz.pfm");
  }
  o.status += "; " + std::to_string(o.written.size()) + " written";
  if(failed)
    o.status += ", " + std::to_string(failed) + " failed";
  return o;
}

RegisterCalibrations::~RegisterCalibrations()
{
  if(m_future.valid())
    m_future.wait();
}

void RegisterCalibrations::operator()()
{
  outputs.done.value.reset();
  if(m_future.valid())
  {
    if(m_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return;
    const auto o = m_future.get();
    outputs.status.value = o.status;
    if(!o.result.ok)
      return;
    outputs.transform.value = o.result.transform;
    outputs.rms.value = float(o.result.rms);
    outputs.inliers.value = o.result.inliers;
    outputs.written.value = o.written;
    outputs.done.value = halp::impulse{};
    return;
  }
  if(!inputs.run)
    return;
  Job job;
  job.reference = inputs.reference.value;
  job.other = inputs.other.value;
  job.move = inputs.move.value;
  job.suffix = inputs.suffix.value;
  job.threshold = inputs.threshold.value;
  job.allowScale = inputs.scale.value;
  job.step = inputs.step.value;
  outputs.status.value = "Registering";
  m_future = std::async(std::launch::async, &run, std::move(job));
}
}
