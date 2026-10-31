#include "ManualProjectorCalibration.hpp"

#include <Gfx/Calibration/EPnP.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <cmath>

namespace Threedim
{
namespace procam = score::gfx::procam;

std::vector<ManualProjectorCalibration::Pair>
ManualProjectorCalibration::parsePairs(std::string_view text)
{
  std::vector<Pair> out;
  for(const auto& entry : splitPaths(text))
  {
    const auto n = splitNumbers(entry);
    if(n.size() < 5)
      continue;
    out.push_back({n[0], n[1], {n[2], n[3], n[4]}});
  }
  return out;
}

std::vector<std::array<float, 4>>
ManualProjectorCalibration::parsePlanes(std::string_view text)
{
  std::vector<std::array<float, 4>> out;
  for(const auto& entry : splitPaths(text))
  {
    const auto n = splitNumbers(entry);
    if(n.size() >= 4)
      out.push_back({n[0], n[1], n[2], n[3]});
  }
  return out;
}

ManualProjectorCalibration::Solution
ManualProjectorCalibration::solve(const std::vector<Pair>& pairs, const Settings& s)
{
  Solution sol;
  std::vector<procam::Vec3> X;
  std::vector<procam::Vec2> x;
  for(const auto& p : pairs)
  {
    X.emplace_back(p.world[0], p.world[1], p.world[2]);
    x.emplace_back(p.u, p.v);
  }
  procam::Options opt;
  procam::Result r;
  if(s.intrinsicsMode == LockIntrinsics)
  {
    procam::Model k;
    if(s.lockedK[0] > 0.f && s.lockedK[1] > 0.f)
    {
      k.fx = s.lockedK[0];
      k.fy = s.lockedK[1];
      k.cx = s.lockedK[2];
      k.cy = s.lockedK[3];
    }
    else if(s.throwRatio > 0.f)
    {
      k.fx = k.fy = double(s.throwRatio) * s.width;
      k.cx = (s.width - 1) * 0.5 - double(s.lensShift[0]) * s.width;
      k.cy = (s.height - 1) * 0.5 + double(s.lensShift[1]) * s.height;
    }
    else
    {
      sol.status = "Lock needs Locked intrinsics or a Throw ratio";
      return sol;
    }
    r = procam::solvePoseAny(X, x, k, opt);
    sol.intrinsicsLocked = true;
  }
  else
  {
    if(pairs.size() < 6)
    {
      sol.status = std::to_string(pairs.size())
                   + " points: 6 are needed to solve the intrinsics (4 with Lock)";
      return sol;
    }
    r = procam::solve(X, x, opt);
  }
  if(!r.ok)
  {
    sol.status = "Failed: " + r.error;
    return sol;
  }
  const auto& m = r.model;
  sol.ok = true;
  sol.intrinsics = {float(m.fx), float(m.fy), float(m.cx), float(m.cy)};
  sol.pose = procam::toColumnMajor(m.worldFromLocal());
  sol.rms = float(r.rms);
  float worst = 0.f;
  for(std::size_t i = 0; i < X.size(); i++)
  {
    procam::Vec2 px;
    if(m.project(X[i], px))
    {
      sol.reprojected.push_back({float(px.x()), float(px.y())});
      sol.errors.push_back(float((px - x[i]).norm()));
    }
    else
    {
      sol.reprojected.push_back({-1.f, -1.f});
      sol.errors.push_back(-1.f);
    }
    worst = std::max(worst, sol.errors.back());
  }
  char buf[256];
  std::snprintf(
      buf, sizeof(buf), "Solved from %d points%s: RMS %.2f px, worst %.2f px, fx %.1f fy %.1f "
                        "cx %.1f cy %.1f",
      int(X.size()), sol.intrinsicsLocked ? " (intrinsics locked)" : "", sol.rms, worst,
      m.fx, m.fy, m.cx, m.cy);
  sol.status = buf;
  return sol;
}

std::vector<float> ManualProjectorCalibration::xyzFromPlanes(
    const Solution& sol, int w, int h, const std::vector<std::array<float, 4>>& planes)
{
  std::vector<float> out(std::size_t(w) * h * 4, 0.f);
  if(!sol.ok || planes.empty())
    return out;
  const Eigen::Matrix4d W = procam::fromColumnMajor(sol.pose);
  const Eigen::Matrix3d R = W.topLeftCorner<3, 3>();
  const Eigen::Vector3d o = W.topRightCorner<3, 1>();
  const auto& k = sol.intrinsics;
  for(int v = 0; v < h; v++)
    for(int u = 0; u < w; u++)
    {
      const Eigen::Vector3d d = R * Eigen::Vector3d((u - k[2]) / k[0], (v - k[3]) / k[1], 1.);
      double best = std::numeric_limits<double>::infinity();
      for(const auto& pl : planes)
      {
        const Eigen::Vector3d n(pl[0], pl[1], pl[2]);
        const double den = n.dot(d);
        if(std::abs(den) < 1e-12)
          continue;
        const double t = -(n.dot(o) + pl[3]) / den;
        if(t > 1e-6 && t < best)
          best = t;
      }
      if(!std::isfinite(best))
        continue;
      const Eigen::Vector3d p = o + best * d;
      float* q = out.data() + (std::size_t(v) * w + u) * 4;
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  return out;
}

bool ManualProjectorCalibration::saveFile(
    const std::string& path, const std::vector<Pair>& pairs, const Solution& sol,
    const Settings& s, const std::vector<std::array<float, 4>>& planes)
{
  const std::string base = SaveFloatTexture::basePath(path);
  if(base.empty())
    return false;
  const QString qbase = QString::fromStdString(base);
  QJsonArray files;
  if(sol.ok && !planes.empty())
  {
    score::gfx::FloatImage img;
    img.width = s.width;
    img.height = s.height;
    img.channels = 4;
    img.data = xyzFromPlanes(sol, s.width, s.height, planes);
    if(!score::gfx::writeFloatImageSplit(qbase, img))
      return false;
    files.append(QFileInfo(qbase + ".xyz.pfm").fileName());
    files.append(QFileInfo(qbase + ".valid.pfm").fileName());
  }

  QJsonObject o;
  o["mode"] = "manual";
  o["width"] = s.width;
  o["height"] = s.height;
  QJsonArray k, pose, pts, errs, pl;
  for(float v : sol.intrinsics)
    k.append(v);
  for(float v : sol.pose)
    pose.append(v);
  for(const auto& p : pairs)
    pts.append(QJsonArray{p.u, p.v, p.world[0], p.world[1], p.world[2]});
  for(float e : sol.errors)
    errs.append(e);
  for(const auto& p : planes)
    pl.append(QJsonArray{p[0], p[1], p[2], p[3]});
  if(sol.ok)
  {
    o["intrinsics"] = k;
    o["worldFromProjector"] = pose;
    o["rms"] = sol.rms;
    o["errors"] = errs;
  }
  o["intrinsicsMode"] = sol.intrinsicsLocked ? "lock" : "solve";
  o["intrinsicsLocked"] = sol.intrinsicsLocked;
  o["viewer"] = QJsonArray{0., 0., 0.};
  o["points"] = pts;
  o["planes"] = pl;
  o["projectorIndex"] = 0;
  o["files"] = files;
  QFile f{qbase + ".json"};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(QJsonDocument(o).toJson());
  return true;
}

ManualProjectorCalibration::Loaded
ManualProjectorCalibration::loadFile(const std::string& path)
{
  Loaded l;
  const QString file = QString::fromStdString(SaveFloatTexture::basePath(path)) + ".json";
  QFile f{file};
  if(!f.open(QIODevice::ReadOnly))
  {
    l.status = "Failed: cannot read " + file.toStdString();
    return l;
  }
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  l.width = o["width"].toInt();
  l.height = o["height"].toInt();
  for(const auto& v : o["points"].toArray())
  {
    const auto a = v.toArray();
    if(a.size() >= 5)
      l.pairs.push_back(
          {float(a[0].toDouble()), float(a[1].toDouble()),
           {float(a[2].toDouble()), float(a[3].toDouble()), float(a[4].toDouble())}});
  }
  const auto k = o["intrinsics"].toArray();
  const auto p = o["worldFromProjector"].toArray();
  if(k.size() == 4 && p.size() == 16)
  {
    for(int i = 0; i < 4; i++)
      l.solution.intrinsics[i] = float(k[i].toDouble());
    for(int i = 0; i < 16; i++)
      l.solution.pose[i] = float(p[i].toDouble());
    l.solution.rms = float(o["rms"].toDouble());
    l.solution.intrinsicsLocked = o["intrinsicsLocked"].toBool();
    l.solution.ok = l.solution.intrinsics[0] > 0.f && l.solution.intrinsics[1] > 0.f;
  }
  l.ok = true;
  l.status = "Loaded " + file.toStdString() + ": " + std::to_string(l.pairs.size())
             + " points";
  return l;
}

std::function<void(ManualProjectorCalibration&)>
ManualProjectorCalibration::ins::load_t::process(file_type data)
{
  auto res = std::make_shared<Loaded>(loadFile(std::string(data.filename)));
  return [res = std::move(res)](ManualProjectorCalibration& self) mutable {
    self.m_loaded = std::move(res);
  };
}

void ManualProjectorCalibration::renderPattern(
    int w, int h, std::array<float, 2> c, int size, float level,
    const std::vector<Pair>& pairs, const Solution& sol, uint8_t* rgba)
{
  QImage img{rgba, w, h, w * 4, QImage::Format_RGBA8888};
  img.fill(Qt::black);
  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing, true);
  const int lv = int(std::lround(std::clamp(level, 0.f, 1.f) * 255.f));
  auto at = [](float u, float v) { return QPointF(u + 0.5, v + 0.5); };

  const int mark = std::max(4, size / 4);
  const bool text = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  for(std::size_t i = 0; i < pairs.size(); i++)
  {
    const QPointF q = at(pairs[i].u, pairs[i].v);
    p.setPen(QPen(QColor(0, lv, 0), 2.));
    p.drawLine(q - QPointF(mark, 0), q + QPointF(mark, 0));
    p.drawLine(q - QPointF(0, mark), q + QPointF(0, mark));
    QString label = QString::number(i + 1);
    if(sol.ok && i < sol.reprojected.size() && sol.errors[i] >= 0.f)
    {
      const QPointF r = at(sol.reprojected[i][0], sol.reprojected[i][1]);
      p.setPen(QPen(QColor(lv, 0, 0), 2.));
      p.drawLine(q, r);
      p.drawEllipse(r, 3., 3.);
      label += QStringLiteral(": %1 px").arg(sol.errors[i], 0, 'f', 1);
    }
    if(text)
    {
      p.setPen(QColor(lv, lv, lv));
      p.drawText(q + QPointF(mark + 2, -mark - 2), label);
    }
  }

  const QPointF x = at(c[0], c[1]);
  p.setPen(QPen(QColor(lv / 3, lv / 3, lv / 3), 1.));
  p.drawLine(QPointF(0, x.y()), QPointF(w, x.y()));
  p.drawLine(QPointF(x.x(), 0), QPointF(x.x(), h));
  p.setPen(QPen(QColor(lv, lv, lv), 1.));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(x, size * 0.5, size * 0.5);
  p.drawLine(x - QPointF(size, 0), x + QPointF(size, 0));
  p.drawLine(x - QPointF(0, size), x + QPointF(0, size));
}

ManualProjectorCalibration::Settings ManualProjectorCalibration::settings() const
{
  Settings s;
  s.width = std::max(1, inputs.projector_size.value.x);
  s.height = std::max(1, inputs.projector_size.value.y);
  s.intrinsicsMode = inputs.intrinsics_mode.value;
  const auto& k = inputs.locked_intrinsics.value;
  s.lockedK = {k.x, k.y, k.z, k.w};
  s.throwRatio = inputs.throw_ratio.value;
  s.lensShift = {inputs.lens_shift.value.x, inputs.lens_shift.value.y};
  return s;
}

void ManualProjectorCalibration::rebuild()
{
  m_all = m_typed;
  m_all.insert(m_all.end(), m_recorded.begin(), m_recorded.end());
  outputs.count.value = int(m_all.size());
  m_dirty = true;
  m_patternDirty = true;
}

void ManualProjectorCalibration::runSolve()
{
  m_solution = solve(m_all, settings());
  m_dirty = false;
  m_patternDirty = true;
  publish();
}

void ManualProjectorCalibration::publish()
{
  outputs.status.value = m_solution.status;
  outputs.errors.value = m_solution.errors;
  if(!m_solution.ok)
    return;
  outputs.intrinsics.value = m_solution.intrinsics;
  outputs.pose.value = m_solution.pose;
  for(int c = 0; c < 4; c++)
  {
    auto& col = c == 0 ? outputs.m0.value
                : c == 1 ? outputs.m1.value
                : c == 2 ? outputs.m2.value
                         : outputs.m3.value;
    for(int k = 0; k < 4; k++)
      col[k] = m_solution.pose[c * 4 + k];
  }
  outputs.rms.value = m_solution.rms;
  const auto planes = parsePlanes(inputs.planes.value);
  if(!planes.empty())
  {
    const auto s = settings();
    const auto xyz = xyzFromPlanes(m_solution, s.width, s.height, planes);
    outputs.xyz.create(s.width, s.height);
    std::copy(xyz.begin(), xyz.end(), outputs.xyz.texture.bytes);
    outputs.xyz.upload();
  }
  outputs.done.value = halp::impulse{};
}

void ManualProjectorCalibration::operator()()
{
  outputs.done.value.reset();
  const auto s = settings();

  if(m_loaded)
  {
    const auto l = std::move(m_loaded);
    if(l->ok)
    {
      m_recorded = l->pairs;
      m_typed.clear();
      rebuild();
      m_solution = l->solution;
      m_dirty = false;
      if(auto fresh = solve(m_all, s); fresh.ok)
        m_solution = std::move(fresh);
      m_solution.status = l->status + (m_solution.ok ? "; " + m_solution.status : "");
      publish();
    }
    else
      outputs.status.value = l->status;
  }

  if(inputs.points.value != m_typedText)
  {
    m_typedText = inputs.points.value;
    m_typed = parsePairs(m_typedText);
    rebuild();
  }

  const float cu = inputs.crosshair.value.x * s.width - 0.5f;
  const float cv = inputs.crosshair.value.y * s.height - 0.5f;
  if(inputs.record)
  {
    const auto& wp = inputs.world.value;
    m_recorded.push_back({cu, cv, {wp.x, wp.y, wp.z}});
    rebuild();
  }
  if(inputs.delete_last && !m_recorded.empty())
  {
    m_recorded.pop_back();
    rebuild();
  }
  if(inputs.clear)
  {
    m_recorded.clear();
    rebuild();
  }

  if(inputs.solve || (m_dirty && inputs.auto_solve.value))
    runSolve();
  else if(m_dirty)
  {
    outputs.status.value = std::to_string(m_all.size()) + " points";
    m_dirty = false;
  }

  if(inputs.save)
  {
    const bool ok = !inputs.save_path.value.empty()
                    && saveFile(
                        inputs.save_path.value, m_all, m_solution, s,
                        parsePlanes(inputs.planes.value));
    outputs.status.value
        = ok ? "Saved " + SaveFloatTexture::basePath(inputs.save_path.value) + ".json"
             : "Failed: cannot save to " + inputs.save_path.value;
  }

  const std::array<float, 2> ch{cu, cv};
  if(m_patternDirty || ch != m_shownCrosshair || s.width != m_shownW
     || s.height != m_shownH || inputs.crosshair_size.value != m_shownSize
     || inputs.level.value != m_shownLevel)
  {
    outputs.pattern.create(s.width, s.height);
    renderPattern(
        s.width, s.height, ch, inputs.crosshair_size.value, inputs.level.value, m_all,
        m_solution, outputs.pattern.texture.bytes);
    outputs.pattern.upload();
    m_patternDirty = false;
    m_shownCrosshair = ch;
    m_shownW = s.width;
    m_shownH = s.height;
    m_shownSize = inputs.crosshair_size.value;
    m_shownLevel = inputs.level.value;
  }
}
}
