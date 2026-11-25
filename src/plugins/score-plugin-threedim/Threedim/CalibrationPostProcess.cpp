#include "CalibrationPostProcess.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/EquirectWarp.hpp>
#include <Threedim/PlaneCompletion.hpp>
#include <Threedim/PoseRefinement.hpp>
#include <Threedim/ProjectorBlend.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Threedim
{
namespace
{
bool cancelled(const std::atomic_bool* c) noexcept
{
  return c && c->load(std::memory_order_relaxed);
}

QString jsonOf(const QString& base)
{
  return base + QStringLiteral(".json");
}

bool saveAs(const QString& base, StructuredLightCore::Result r, bool screen, QString* err)
{
  r.screen = screen;
  return StructuredLightCore::saveFile(jsonOf(base), r, err);
}
}

QString CalibrationPostProcess::label(const Options& o, int k)
{
  return QString::number((k < (int)o.outputs.size() ? o.outputs[k] : k) + 1);
}

bool CalibrationPostProcess::saveResults(
    const Results& results, const Options& o, QStringList& log)
{
  bool all = true;
  for(int i = 0; i < (int)results.size() && i < o.names.size(); i++)
    if(const auto& r = results[i]; r && r->ok && !r->fromFile)
    {
      QDir{}.mkpath(QFileInfo{o.names[i]}.absolutePath());
      QString err;
      if(!saveAs(o.names[i], *r, o.screens, &err))
      {
        log.push_back(tr("Output %1: cannot write %2 (%3)")
                          .arg(label(o, i), jsonOf(o.names[i]), err));
        all = false;
      }
    }
  return all;
}

CalibrationPostProcess::Results
CalibrationPostProcess::load(const QStringList& names, QStringList& log)
{
  Results out;
  for(int i = 0; i < names.size(); i++)
  {
    const QString& base = names[i];
    auto r = std::make_shared<StructuredLightCore::Result>(
        StructuredLightCore::loadFile(jsonOf(base)));
    if(!r->ok)
    {
      log.push_back(tr("Output %1: %2").arg(i + 1).arg(QString::fromStdString(r->status)));
      out.push_back(nullptr);
      continue;
    }
    const QString measured = base + QStringLiteral(".measured.xyz.pfm");
    if(QFile::exists(measured))
    {
      const auto img = score::gfx::loadFloatImageRGBA(measured);
      if(img.valid() && img.width == r->width && img.height == r->height)
        r->xyz = img.data;
    }
    r->projectorIndex = i;
    out.push_back(std::move(r));
  }
  return out;
}

RoomGeometry CalibrationPostProcess::room(const Options& o, QStringList& log)
{
  if(!o.viewer)
    return {};
  if(o.geometry == Geometry::Box)
  {
    const auto& b = o.box;
    return RoomGeometry::box(b[0], b[1], b[2], b[3], b[4], b[5]);
  }
  if(o.geometry == Geometry::Model)
  {
    QString err;
    auto g = RoomGeometry::load(o.model, &err);
    if(g.empty())
      log.push_back(tr("Room model: %1").arg(err));
    else
      log.push_back(tr("Room model: %1 triangles").arg(g.triangleCount()));
    return g;
  }
  return {};
}

CalibrationPostProcess::Measured
CalibrationPostProcess::measure(const Results& results, const Options& o, bool fresh)
{
  Measured m;
  const int n = std::min<int>(int(results.size()), int(o.names.size()));
  m.results.resize(n);
  m.cameras.resize(n);
  for(int i = 0; i < n; i++)
  {
    if(!results[i] || !results[i]->ok)
      continue;
    auto& r = m.results[i];
    r = *results[i];
    if(!o.viewer)
      continue;
    const QString& base = o.names[i];
    if(!fresh)
    {
      m.cameras[i] = loadMeasuredCamera(base);
      continue;
    }
    for(const char* ext : {".xyz.pfm", ".valid.pfm"})
    {
      const QString to = base + QStringLiteral(".measured") + QString::fromLatin1(ext);
      QFile::remove(to);
      QFile::copy(base + QString::fromLatin1(ext), to);
    }
    MeasuredCamera mc;
    for(int k = 0; k < 4; k++)
      mc.intrinsics[k] = r.cameraIntrinsics[k];
    mc.lens = r.lens;
    mc.width = r.camWidth;
    mc.height = r.camHeight;
    saveMeasuredCamera(base, mc);
    m.cameras[i] = mc;
  }
  return m;
}

namespace
{
MeasuredCamera target(
    const MeasuredCamera& measured, const CalibrationPostProcess::Options& o,
    const CameraLens& lens)
{
  MeasuredCamera t;
  t.lens = lens;
  t.width = measured.width;
  t.height = measured.height;
  const auto& k = o.cameraIntrinsics;
  if(k[0] > 0.f && k[1] > 0.f)
    t.intrinsics = {k[0], k[1], k[2], k[3]};
  else
    t.intrinsics = CameraLens::intrinsicsFromFov(lens.model, o.fov, t.width, t.height);
  return t;
}
}

std::optional<CameraLens>
CalibrationPostProcess::estimateLens(const Measured& m, const Options& o, QStringList& log)
{
  if(!o.viewer || !o.estimateLens || !o.completePlanes)
    return std::nullopt;
  std::vector<std::vector<float>> maps;
  std::vector<std::array<int, 2>> sizes;
  int ref = -1;
  for(int i = 0; i < (int)m.results.size(); i++)
    if(m.cameras[i] && m.results[i].ok)
    {
      maps.push_back(m.results[i].xyz);
      sizes.push_back({m.results[i].width, m.results[i].height});
      if(ref < 0)
        ref = i;
    }
  if(ref < 0)
    return std::nullopt;
  const auto model = o.lens.model == CameraLens::Pinhole ? CameraLens::Distorted : o.lens.model;
  CameraLens probe;
  probe.model = model;
  const auto e = estimateRadialDistortion(
      maps, sizes, *m.cameras[ref], model, target(*m.cameras[ref], o, probe).intrinsics,
      o.worldFromCamera);
  if(!e.ok)
  {
    log.push_back(tr("Lens: not estimated"));
    return std::nullopt;
  }
  CameraLens lens;
  lens.model = model;
  lens.k[0] = e.k1;
  log.push_back(tr("Lens: %1 k1 = %2 (plane residual %3 instead of %4)")
                    .arg(QString::fromLatin1(CameraLens::name(model)))
                    .arg(e.k1, 0, 'f', 4)
                    .arg(e.residual, 0, 'f', 3)
                    .arg(e.residualAtZero, 0, 'f', 3));
  return lens;
}

void CalibrationPostProcess::convertLens(Measured& m, const Options& o, const CameraLens& lens)
{
  for(int i = 0; i < (int)m.results.size(); i++)
  {
    auto& r = m.results[i];
    if(!r.ok || !m.cameras[i])
      continue;
    const auto to = target(*m.cameras[i], o, lens);
    if(!(to.lens == m.cameras[i]->lens) || to.intrinsics != m.cameras[i]->intrinsics)
      r.xyz = ::Threedim::convertLens(r.xyz, *m.cameras[i], to, o.worldFromCamera);
    for(int k = 0; k < 4; k++)
      r.cameraIntrinsics[k] = float(to.intrinsics[k]);
    r.lens = to.lens;
  }
}

void CalibrationPostProcess::align(Measured& m, const Options& o, QStringList& log)
{
  if(!o.viewer || o.anchors.empty())
    return;
  const int n = int(m.results.size());
  std::vector<std::pair<int, StructuredLightCore::Result>> earlier;
  QStringList used;
  for(const auto& a : o.anchors)
  {
    if(a.projector < 0 || a.projector >= n || !m.results[a.projector].ok)
      continue;
    auto prev = StructuredLightCore::loadFile(a.calibration);
    if(!prev.ok)
    {
      log.push_back(tr("Output %1: %2")
                        .arg(label(o, a.projector))
                        .arg(QString::fromStdString(prev.status)));
      continue;
    }
    const auto& r = m.results[a.projector];
    if(prev.width != r.width || prev.height != r.height)
    {
      log.push_back(tr("Output %1: its earlier calibration is %2x%3, not %4x%5")
                        .arg(label(o, a.projector))
                        .arg(prev.width)
                        .arg(prev.height)
                        .arg(r.width)
                        .arg(r.height));
      continue;
    }
    earlier.emplace_back(a.projector, std::move(prev));
    used.push_back(label(o, a.projector));
  }
  std::vector<std::pair<const std::vector<float>*, const std::vector<float>*>> pairs;
  for(const auto& [p, prev] : earlier)
    pairs.push_back({&m.results[p].xyz, &prev.xyz});
  const auto al = alignDirections(pairs);
  if(!al.ok)
  {
    log.push_back(tr("Not aligned: the outputs to align to share too few measured pixels "
                     "with this run"));
    return;
  }
  for(auto& r : m.results)
    if(r.ok)
      rotateDirections(r.xyz, al.rotation);
  log.push_back(tr("Aligned to the earlier calibration of output(s) %1: camera turned %2 "
                   "deg, %3' left (median over %4 of %5 pixels)")
                    .arg(used.join(QStringLiteral(", ")))
                    .arg(al.angleDeg, 0, 'f', 2)
                    .arg(al.residualArcmin, 0, 'f', 2)
                    .arg(al.inliers)
                    .arg(al.pairs));
}

std::vector<std::vector<float>>
CalibrationPostProcess::completePlanes(const Measured& m, const Options& o, QStringList& log)
{
  std::vector<std::vector<float>> out(m.results.size());
  for(int i = 0; i < (int)m.results.size(); i++)
  {
    const auto& r = m.results[i];
    if(!r.ok)
      continue;
    out[i] = r.xyz;
    if(!o.viewer || !o.completePlanes)
      continue;
    const auto c = completeFromPlanes(r.xyz, r.width, r.height, o.planeThresholdArcmin, 2);
    if(!c.ok)
    {
      log.push_back(tr("Output %1: not completed from planes (%2)")
                        .arg(label(o, i))
                        .arg(QString::fromStdString(c.status)));
      continue;
    }
    QStringList residuals;
    for(double a : c.medianArcmin)
      residuals.push_back(QStringLiteral("%1'").arg(a, 0, 'f', 1));
    log.push_back(tr("Output %1: %2 plane(s), median residual %3, %4% of the measured "
                     "pixels explained")
                      .arg(label(o, i))
                      .arg(int(c.homographies.size()))
                      .arg(residuals.join(QStringLiteral(" / ")))
                      .arg(c.explained * 100., 0, 'f', 1));
    auto completed = r;
    completed.xyz = c.directions;
    completed.coverage = 100.f;
    completed.status = r.status + " | completed from " + c.status;
    QString err;
    if(saveAs(o.names[i], completed, o.screens, &err))
      out[i] = std::move(completed.xyz);
    else
      log.push_back(tr("Output %1: cannot write the completed map (%2)").arg(label(o, i), err));
  }
  return out;
}

std::optional<std::array<float, 6>> CalibrationPostProcess::refinePose(
    const Measured& m, const std::vector<std::vector<float>>& directions,
    const RoomGeometry& room, const Options& o, QStringList& log)
{
  if(!o.viewer || room.empty() || !o.refinePose)
    return std::nullopt;
  std::vector<DirectionMap> dm;
  for(int i = 0; i < (int)m.results.size(); i++)
    if(m.results[i].ok)
      dm.push_back({&directions[i], m.results[i].width, m.results[i].height});
  const auto fit = refineCameraPose(dm, room, o.cameraPosition, o.cameraRotation, true);
  if(!fit.ok)
  {
    log.push_back(tr("Camera pose not refined: %1").arg(QString::fromStdString(fit.status)));
    return std::nullopt;
  }
  const auto& p = fit.position;
  const auto& q = fit.rotation;
  log.push_back(tr("Camera pose refined over %1 output(s): position %2 %3 %4 m, rotation "
                   "%5 %6 %7 deg; projector fit %8 -> %9 px")
                    .arg(fit.outputsUsed)
                    .arg(p[0], 0, 'f', 3)
                    .arg(p[1], 0, 'f', 3)
                    .arg(p[2], 0, 'f', 3)
                    .arg(q[0], 0, 'f', 2)
                    .arg(q[1], 0, 'f', 2)
                    .arg(q[2], 0, 'f', 2)
                    .arg(fit.residualBefore, 0, 'f', 2)
                    .arg(fit.residualAfter, 0, 'f', 2));
  if(fit.outputsPlanar > 0)
    log.push_back(tr("%1 output(s) on a single plane left out of the pose").arg(fit.outputsPlanar));
  return std::array<float, 6>{p[0], p[1], p[2], q[0], q[1], q[2]};
}

void CalibrationPostProcess::writeMaps(
    const Measured& m, const std::vector<std::vector<float>>& directions,
    const RoomGeometry& room, const std::array<float, 6>& pose, const Options& o,
    QStringList& log)
{
  const std::array<float, 3> camPos{pose[0], pose[1], pose[2]};
  const std::array<float, 3> camRot{pose[3], pose[4], pose[5]};
  const bool equirect = o.viewer && o.projection == Gfx::CanvasProjection::Equirectangular;
  std::array<double, 9> R{};
  {
    const auto r = eulerRotation(camRot);
    for(int a = 0; a < 3; a++)
      for(int b = 0; b < 3; b++)
        R[a * 3 + b] = r(a, b);
  }
  int warps = 0;
  for(int i = 0; i < (int)m.results.size(); i++)
  {
    const auto& r = m.results[i];
    if(!r.ok)
      continue;
    const QString& base = o.names[i];
    const QString name = QFileInfo{base}.fileName();
    QString err;
    bool points = false;
    if(!o.viewer)
      points = savePointsMap(base, r.xyz, r.width, r.height, &err);
    else if(!room.empty())
    {
      const auto cast = castDirections(directions[i], r.width, r.height, room, camPos, camRot);
      points = savePointsMap(base, cast.points, r.width, r.height, &err);
      if(points)
      {
        QString line = tr("Output %1: %2% of the directions meet the room")
                           .arg(label(o, i))
                           .arg(cast.hitFraction * 100., 0, 'f', 1);
        if(const double fit = projectorFitResidual(cast.points, r.width, r.height); fit >= 0.)
          line += tr(", projector fit %1 px").arg(fit, 0, 'f', 2);
        log.push_back(line);
      }
    }
    if(!err.isEmpty())
      log.push_back(tr("Output %1: points map not written (%2)").arg(label(o, i), err));
    if(!points)
      forgetCalibrationFiles(
          base, {name + QStringLiteral(".points.pfm"), name + QStringLiteral(".points.valid.pfm")});

    bool warp = false;
    if(equirect)
    {
      const QRectF src = i < int(o.sourceRects.size()) ? o.sourceRects[i] : QRectF{0, 0, 1, 1};
      const auto w = equirectWarp(
          directions[i], r.width, r.height, src, room.empty() ? nullptr : &R);
      err.clear();
      warp = saveEquirectWarp(base, w, &err);
      if(warp)
        warps++;
      else if(w.enclosesPole)
        log.push_back(tr("Output %1 goes round the canvas's pole: no warp map; give the room "
                         "(box or model) for its points map")
                          .arg(label(o, i)));
      else
        log.push_back(tr("Output %1: warp map not written (%2)").arg(label(o, i), err));
    }
    if(!warp)
      forgetCalibrationFiles(
          base, {name + QStringLiteral(".warp.pfm"), name + QStringLiteral(".warp.valid.pfm")});
  }
  if(warps > 0)
    log.push_back(tr("Equirectangular warp maps written: %1").arg(warps));
}

void CalibrationPostProcess::blend(const Options& o, QStringList& log)
{
  ProjectorBlendMaps::Job job;
  if(o.blendCalibrations.isEmpty())
    for(const auto& n : o.names)
      job.paths.push_back(jsonOf(n).toStdString());
  else
    for(const auto& c : o.blendCalibrations)
      job.paths.push_back(c.toStdString());
  job.settings.ramp = {0.f};
  job.settings.exponent = {1.f};
  job.settings.blackCompensation = o.blackCompensation;
  const auto b = ProjectorBlendMaps::run(job);
  log.push_back(tr("Projector Blend Maps: %1").arg(QString::fromStdString(b.status)));
}

CalibrationPostProcess::Outcome CalibrationPostProcess::run(
    const Results& results, const Options& o, bool fresh, const std::atomic_bool* cancel,
    std::atomic<float>* progress)
{
  Outcome out;
  float done = 0.f;
  auto stopped = [&] {
    if(progress)
      progress->store(done += 0.125f, std::memory_order_relaxed);
    if(!cancelled(cancel))
      return false;
    out.status = tr("Aborted");
    out.log.push_back(out.status);
    return true;
  };
  if(fresh)
    saveResults(results, o, out.log);
  if(stopped())
    return out;
  const auto g = room(o, out.log);
  auto m = measure(results, o, fresh);
  if(o.viewer)
  {
    out.lens = estimateLens(m, o, out.log);
    if(stopped())
      return out;
    convertLens(m, o, out.lens ? *out.lens : o.lens);
    align(m, o, out.log);
  }
  if(stopped())
    return out;
  const auto directions = completePlanes(m, o, out.log);
  if(stopped())
    return out;
  out.pose = refinePose(m, directions, g, o, out.log);
  const auto& p = o.cameraPosition;
  const auto& q = o.cameraRotation;
  const std::array<float, 6> pose
      = out.pose ? *out.pose : std::array<float, 6>{p[0], p[1], p[2], q[0], q[1], q[2]};
  if(stopped())
    return out;
  writeMaps(m, directions, g, pose, o, out.log);
  if(stopped())
    return out;
  if(o.blend && !o.screens)
    blend(o, out.log);
  if(progress)
    progress->store(1.f, std::memory_order_relaxed);
  out.ok = true;
  out.status = tr("Done");
  return out;
}
}
