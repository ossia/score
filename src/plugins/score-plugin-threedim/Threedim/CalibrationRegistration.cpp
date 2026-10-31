#include "CalibrationRegistration.hpp"

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>
#include <Threedim/FloatTextureIO.hpp>

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

void RegisterCalibrations::operator()()
{
  outputs.done.value.reset();
  if(!inputs.run)
    return;
  const auto a = loadCalibration(inputs.reference.value);
  const auto b = loadCalibration(inputs.other.value);
  const auto r = solve(a, b, inputs.threshold.value, inputs.scale.value, inputs.step.value);
  outputs.status.value = r.status;
  if(!r.ok)
    return;
  outputs.transform.value = r.transform;
  outputs.rms.value = float(r.rms);
  outputs.inliers.value = r.inliers;
  outputs.written.value.clear();
  int failed = 0;
  for(const auto& path : splitPaths(inputs.move.value))
  {
    auto c = loadCalibration(path);
    if(!c.ok())
    {
      failed++;
      continue;
    }
    const std::string src = c.base;
    apply(c, r.transform);
    c.base = src + inputs.suffix.value;
    if(c.base == src || !saveCalibration(c, src))
    {
      failed++;
      continue;
    }
    outputs.written.value.push_back(c.base + ".xyz.pfm");
  }
  outputs.status.value += "; " + std::to_string(outputs.written.value.size()) + " written";
  if(failed)
    outputs.status.value += ", " + std::to_string(failed) + " failed";
  outputs.done.value = halp::impulse{};
}
}
