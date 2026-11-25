#include "ViewerFrustum.hpp"

#include <Gfx/Calibration/OffAxis.hpp>
#include <Gfx/Graph/WarpImport.hpp>
#include <Gfx/Window/CalibrationSet.hpp>

#include <cmath>
#include <cstdio>

namespace Threedim
{
namespace oa = score::gfx::offaxis;

namespace
{
std::string rows(const Eigen::Matrix4d& m)
{
  std::string out;
  char buf[32];
  for(int r = 0; r < 4; r++)
    for(int c = 0; c < 4; c++)
    {
      std::snprintf(buf, sizeof(buf), "%s%.9g", r + c == 0 ? "" : " ", m(r, c));
      out += buf;
    }
  return out;
}

std::array<float, 16> columnMajor(const Eigen::Matrix4d& m)
{
  std::array<float, 16> out;
  for(int c = 0; c < 4; c++)
    for(int r = 0; r < 4; r++)
      out[c * 4 + r] = float(m(r, c));
  return out;
}
}

ProjectorViewerFrustum::Result ProjectorViewerFrustum::solve(
    const std::vector<float>& xyz, int w, int h, const Settings& s)
{
  Result out;
  const oa::Vec3 pe(s.viewer[0], s.viewer[1], s.viewer[2]);
  const auto r = oa::fitViewerFrustum(xyz, w, h, pe, s.planeTolerance, s.oversampling);
  if(!r.ok)
  {
    out.status = "Failed: " + r.error;
    return out;
  }
  if(!(s.nearPlane > 0.f) || !(s.farPlane > s.nearPlane))
  {
    out.status = "Failed: Near must be positive and below Far";
    return out;
  }
  auto v3 = [](const oa::Vec3& v) {
    return std::array<float, 3>{float(v.x()), float(v.y()), float(v.z())};
  };
  out.pa = v3(r.pa);
  out.pb = v3(r.pb);
  out.pc = v3(r.pc);
  const auto& f = r.frustum;
  out.frustum = {float(f.l), float(f.r), float(f.b), float(f.t)};
  const Eigen::Matrix4d V = f.view(pe);
  Eigen::Matrix4d P = f.projection(s.nearPlane, s.farPlane);
  P.row(2) = -P.row(2);
  out.view = columnMajor(V);
  out.projection = columnMajor(P);
  out.poseRows = rows(f.worldFromEye(pe));
  out.projectionRows = rows(P);
  out.width = r.width;
  out.height = r.height;
  out.planar = r.planar;
  char buf[240];
  std::snprintf(
      buf, sizeof(buf),
      "Done: %s, %.1f x %.1f deg, render %d x %d (projector pixels %.3f arcmin median, "
      "%.3f at the 10th percentile)",
      r.planar ? "planar" : "curved", (std::atan(f.r) - std::atan(f.l)) * 180. / M_PI,
      (std::atan(f.t) - std::atan(f.b)) * 180. / M_PI, r.width, r.height,
      r.pixelMedian * 180. * 60. / M_PI, r.pixelPercentile * 180. * 60. / M_PI);
  out.status = buf;
  out.ok = true;
  return out;
}

std::shared_ptr<const CalibrationFile>
ProjectorViewerFrustum::Cache::get(const QString& file)
{
  const QString base = Gfx::calibrationBase(file);
  const QString stamp = score::gfx::mapFileStamp(base + QStringLiteral(".json")) + ';'
                        + score::gfx::mapFileStamp(
                            calibrationMapFile(base, QStringLiteral(".xyz.pfm")));
  std::lock_guard l{mutex};
  if(!calibration || path != base || stamp != this->stamp)
  {
    calibration = std::make_shared<const CalibrationFile>(loadCalibration(base));
    path = base;
    this->stamp = stamp;
    loads++;
  }
  return calibration;
}

void ProjectorViewerFrustum::worker::work(
    std::weak_ptr<State> state, uint64_t id, std::shared_ptr<const Job> job)
{
  auto s = state.lock();
  if(!s)
    return;
  Result r;
  if(!job->file.isEmpty())
  {
    const auto c = s->cache.get(job->file);
    r = c->ok() ? solve(c->xyz, c->width, c->height, job->settings)
                : Result{.status = "Failed: " + c->error.toStdString()};
  }
  else if(job->xyz)
    r = solve(*job->xyz, job->width, job->height, job->settings);
  s->jobs.finish(id, std::move(r));
}

ProjectorViewerFrustum::Settings ProjectorViewerFrustum::settings() const
{
  return {
      {inputs.viewer.value.x, inputs.viewer.value.y, inputs.viewer.value.z},
      inputs.plane_tolerance.value,
      inputs.oversampling.value,
      inputs.near_plane.value,
      inputs.far_plane.value};
}

void ProjectorViewerFrustum::request(Job job)
{
  m_requested = job.settings;
  m_requestedFile = job.file;
  m_sinceCheck = 0;
  worker.request(
      m_state, m_state->jobs.start(), std::make_shared<const Job>(std::move(job)));
}

void ProjectorViewerFrustum::operator()()
{
  outputs.done.value.reset();
  auto& jobs = m_state->jobs;
  if(auto r = jobs.fresh(m_seen))
  {
    outputs.status.value = r->status;
    if(r->ok)
    {
      auto v3 = [](const std::array<float, 3>& v) {
        return halp::xyz_type<float>{v[0], v[1], v[2]};
      };
      outputs.pa.value = v3(r->pa);
      outputs.pb.value = v3(r->pb);
      outputs.pc.value = v3(r->pc);
      outputs.frustum.value = r->frustum;
      outputs.view.value = r->view;
      outputs.projection.value = r->projection;
      outputs.pose_rows.value = r->poseRows;
      outputs.projection_rows.value = r->projectionRows;
      outputs.size.value = {r->width, r->height};
      outputs.planar.value = r->planar;
      outputs.done.value = halp::impulse{};
    }
  }

  const auto s = settings();
  const QString file = QString::fromStdString(inputs.file.value);
  const bool continuous = inputs.continuous.value;
  const bool changed = !m_requested || !(*m_requested == s) || m_requestedFile != file;
  const bool recheck = continuous && ++m_sinceCheck >= recheckFrames;
  const bool wanted = bool(inputs.compute) || (continuous && (changed || recheck));

  if(!file.isEmpty())
  {
    inputs.xyz.request_readback = false;
    m_wait = -1;
    if(wanted && !jobs.busy())
      request(Job{.file = file, .settings = s});
    return;
  }

  auto& in = inputs.xyz;
  if(wanted && m_wait < 0 && !jobs.busy())
  {
    in.request_readback = true;
    m_wait = 0;
    return;
  }
  if(m_wait < 0)
    return;
  const auto& t = in.texture;
  if(t.bytes && t.width > 0 && t.height > 0)
  {
    in.request_readback = false;
    m_wait = -1;
    if(in.resampled)
    {
      outputs.status.value = "Failed: XYZ map is not an RGBA32F texture it can read at its size";
      m_requested = s;
      m_requestedFile = file;
      return;
    }
    request(
        Job{.xyz = std::make_shared<const std::vector<float>>(
                t.bytes, t.bytes + std::size_t(t.width) * t.height * 4),
            .width = t.width,
            .height = t.height,
            .settings = s});
  }
  else if(++m_wait > readbackFrames)
  {
    in.request_readback = false;
    m_wait = -1;
    m_requested = s;
    m_requestedFile = file;
    outputs.status.value = "Failed: no XYZ map on the input";
  }
}
}
