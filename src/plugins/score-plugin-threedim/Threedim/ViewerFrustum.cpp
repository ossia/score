#include "ViewerFrustum.hpp"

#include <Gfx/Calibration/OffAxis.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <cmath>
#include <cstdio>

namespace Threedim
{
namespace oa = score::gfx::offaxis;

bool ProjectorViewerFrustum::solve(const std::vector<float>& xyz, int w, int h)
{
  const oa::Vec3 pe(inputs.viewer.value.x, inputs.viewer.value.y, inputs.viewer.value.z);
  const auto r = oa::fitViewerFrustum(
      xyz, w, h, pe, inputs.plane_tolerance.value, inputs.oversampling.value);
  if(!r.ok)
  {
    outputs.status.value = "Failed: " + r.error;
    return false;
  }
  auto v3 = [](const oa::Vec3& v) {
    return halp::xyz_type<float>{float(v.x()), float(v.y()), float(v.z())};
  };
  outputs.pa.value = v3(r.pa);
  outputs.pb.value = v3(r.pb);
  outputs.pc.value = v3(r.pc);
  const auto& f = r.frustum;
  outputs.frustum.value = {float(f.l), float(f.r), float(f.b), float(f.t)};
  const Eigen::Matrix4d V = f.view(pe);
  const Eigen::Matrix4d P = f.projection(inputs.near_plane.value, inputs.far_plane.value);
  for(int c = 0; c < 4; c++)
    for(int k = 0; k < 4; k++)
    {
      outputs.view.value[c * 4 + k] = float(V(k, c));
      outputs.projection.value[c * 4 + k] = float(P(k, c));
    }
  outputs.size.value = {r.width, r.height};
  outputs.planar.value = r.planar;
  char buf[240];
  std::snprintf(
      buf, sizeof(buf),
      "Done: %s, %.1f x %.1f deg, render %d x %d (projector pixels %.3f arcmin median, "
      "%.3f at the 10th percentile)",
      r.planar ? "planar" : "curved",
      (std::atan(f.r) - std::atan(f.l)) * 180. / M_PI,
      (std::atan(f.t) - std::atan(f.b)) * 180. / M_PI, r.width, r.height,
      r.pixelMedian * 180. * 60. / M_PI, r.pixelPercentile * 180. * 60. / M_PI);
  outputs.status.value = buf;
  outputs.done.value = halp::impulse{};
  return true;
}

void ProjectorViewerFrustum::operator()()
{
  outputs.done.value.reset();
  const bool trigger = bool(inputs.compute) || inputs.continuous.value;
  if(trigger && !inputs.file.value.empty())
  {
    const auto c = loadCalibration(inputs.file.value);
    if(!c.ok())
      outputs.status.value = "Failed: " + c.error;
    else
      solve(c.xyz, c.width, c.height);
    return;
  }
  if(trigger && !inputs.xyz.request_readback)
  {
    inputs.xyz.request_readback = true;
    m_wait = 0;
    return;
  }
  if(!inputs.xyz.request_readback)
    return;
  const auto& t = inputs.xyz.texture;
  if(t.bytes && t.width > 0 && t.height > 0)
  {
    solve(
        std::vector<float>(t.bytes, t.bytes + std::size_t(t.width) * t.height * 4), t.width,
        t.height);
    inputs.xyz.request_readback = inputs.continuous.value;
  }
  else if(++m_wait > 30)
  {
    outputs.status.value = "Failed: no XYZ map on the input";
    inputs.xyz.request_readback = false;
  }
}
}
