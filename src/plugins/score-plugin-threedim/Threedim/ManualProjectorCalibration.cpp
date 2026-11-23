#include "ManualProjectorCalibration.hpp"

#include <Gfx/Calibration/EPnP.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <cmath>
#include <cstdio>

namespace Threedim
{
namespace procam = score::gfx::procam;

namespace
{
template <std::size_t N>
std::vector<std::array<float, N>>
parseEntries(std::string_view text, const char* what, std::string* error)
{
  std::vector<std::array<float, N>> out;
  std::string bad;
  int k = 0;
  for(const auto& entry : splitPaths(text))
  {
    k++;
    const auto n = parseNumbers(entry);
    if(!n || n->size() != N)
    {
      bad += (bad.empty() ? "" : ", ") + std::to_string(k);
      continue;
    }
    std::array<float, N> a;
    std::copy_n(n->begin(), N, a.begin());
    out.push_back(a);
  }
  if(error)
    *error = bad.empty() ? std::string{} : std::string(what) + ": entry " + bad + " ignored";
  return out;
}
}

std::vector<ManualProjectorCalibration::Pair>
ManualProjectorCalibration::parsePairs(std::string_view text, std::string* error)
{
  std::vector<Pair> out;
  for(const auto& a : parseEntries<5>(text, "Points", error))
    out.push_back({a[0], a[1], {a[2], a[3], a[4]}});
  return out;
}

std::vector<ManualProjectorCalibration::Plane>
ManualProjectorCalibration::parsePlanes(std::string_view text, std::string* error)
{
  return parseEntries<4>(text, "Surface planes", error);
}

std::string ManualProjectorCalibration::formatPairs(const std::vector<Pair>& pairs)
{
  std::string out;
  char buf[160];
  for(const auto& p : pairs)
  {
    std::snprintf(
        buf, sizeof(buf), "%s%.9g %.9g %.9g %.9g %.9g", out.empty() ? "" : "; ", p.u, p.v,
        p.world[0], p.world[1], p.world[2]);
    out += buf;
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
    const Solution& sol, int w, int h, const std::vector<Plane>& planes)
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

ManualProjectorCalibration::Saved ManualProjectorCalibration::saveFile(
    const std::string& path, const std::vector<Pair>& pairs, const Solution& sol,
    const Settings& s, const std::vector<Plane>& planes)
{
  Saved r;
  CalibrationWrite w;
  w.base = Gfx::calibrationBase(QString::fromStdString(path));
  if(sol.ok && !planes.empty())
  {
    score::gfx::FloatImage img;
    img.width = s.width;
    img.height = s.height;
    img.channels = 4;
    img.data = xyzFromPlanes(sol, s.width, s.height, planes);
    w.maps.push_back({".xyz.pfm", std::move(img)});
  }
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
  w.fields["mode"] = "manual";
  w.fields["width"] = s.width;
  w.fields["height"] = s.height;
  if(sol.ok)
  {
    w.fields["intrinsics"] = k;
    w.fields["worldFromProjector"] = pose;
    w.fields["rms"] = sol.rms;
    w.fields["errors"] = errs;
  }
  else
    w.removeKeys = {"intrinsics", "worldFromProjector", "rms", "errors"};
  w.fields["intrinsicsMode"] = sol.intrinsicsLocked ? "lock" : "solve";
  w.fields["intrinsicsLocked"] = sol.intrinsicsLocked;
  w.fields["points"] = pts;
  w.fields["planes"] = pl;
  w.keepListed = {".lum.pfm", ".black.pfm"};
  QString err;
  r.ok = writeCalibration(w, &err);
  r.status = r.ok ? "Saved " + QString(w.base + ".json").toStdString()
                  : "Failed: " + err.toStdString();
  return r;
}

ManualProjectorCalibration::Loaded
ManualProjectorCalibration::loadFile(const QString& filename, const QByteArray& bytes)
{
  Loaded l;
  QJsonParseError perr{};
  const auto doc = QJsonDocument::fromJson(bytes, &perr);
  if(!doc.isObject())
  {
    l.status = "Failed: " + filename.toStdString() + " is not a calibration: "
               + perr.errorString().toStdString();
    return l;
  }
  const auto o = doc.object();
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
  l.status = "Loaded " + filename.toStdString() + ": " + std::to_string(l.pairs.size())
             + " points";
  return l;
}

std::function<void(ManualProjectorCalibration&)>
ManualProjectorCalibration::ins::load_t::process(file_type data)
{
  LoadJob job{
      QString::fromUtf8(data.filename.data(), data.filename.size()),
      QByteArray(data.bytes.data(), qsizetype(data.bytes.size()))};
  return [job = std::move(job)](ManualProjectorCalibration& self) mutable {
    auto& sh = self.shared();
    if(sh.solves.claim(&self))
      self.request(std::move(job), sh.loads.start());
  };
}

void ManualProjectorCalibration::worker::work(
    std::weak_ptr<Shared> shared, uint64_t id, std::shared_ptr<const Job> job)
{
  std::visit(
      [&]<typename J>(const J& j) {
    if constexpr(std::is_same_v<J, SolveJob>)
    {
      Solved r;
      r.solution = solve(j.pairs, j.settings);
      r.width = j.settings.width;
      r.height = j.settings.height;
      if(r.solution.ok && !j.planes.empty())
        r.xyz = xyzFromPlanes(r.solution, r.width, r.height, j.planes);
      if(auto s = shared.lock())
        s->solves.finish(id, std::move(r));
    }
    else if constexpr(std::is_same_v<J, SaveJob>)
    {
      auto r = saveFile(j.path, j.pairs, j.solution, j.settings, j.planes);
      if(auto s = shared.lock())
        s->saves.finish(id, std::move(r));
    }
    else if constexpr(std::is_same_v<J, LoadJob>)
    {
      auto r = loadFile(j.filename, j.bytes);
      if(auto s = shared.lock())
        s->loads.finish(id, std::move(r));
    }
    else
    {
      Pattern r;
      r.width = j.width;
      r.height = j.height;
      r.rgba.resize(std::size_t(j.width) * j.height * 4);
      renderPattern(
          j.width, j.height, j.crosshair, j.size, j.level, j.pairs, j.solution,
          r.rgba.data());
      if(auto s = shared.lock())
        s->patterns.finish(id, std::move(r));
    }
  },
      *job);
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

ManualProjectorCalibration::~ManualProjectorCalibration()
{
  if(node_state)
    node_state->solves.release(this);
}

ManualProjectorCalibration::Shared& ManualProjectorCalibration::shared()
{
  if(!node_state)
    node_state = std::make_shared<Shared>();
  return *node_state;
}

void ManualProjectorCalibration::request(Job job, uint64_t id)
{
  worker.request(node_state, id, std::make_shared<const Job>(std::move(job)));
}

void ManualProjectorCalibration::publish(const Solved& r)
{
  m_solution = r.solution;
  m_patternWanted = true;
  outputs.status.value
      = m_inputError.empty() ? m_solution.status : m_inputError + "; " + m_solution.status;
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
  if(!r.xyz.empty())
  {
    outputs.xyz.create(r.width, r.height);
    std::copy(r.xyz.begin(), r.xyz.end(), outputs.xyz.texture.bytes);
    outputs.xyz.upload();
  }
  outputs.done.value = halp::impulse{};
}

void ManualProjectorCalibration::act(const Settings& s)
{
  auto& sh = shared();
  bool forceSolve = bool(inputs.solve);

  if(auto l = sh.loads.fresh(m_loadedSeen))
  {
    outputs.status.value = l->status;
    if(l->ok)
    {
      auto pairs = l->pairs;
      if(l->width > 0 && l->height > 0 && (l->width != s.width || l->height != s.height))
      {
        const float sx = float(s.width) / l->width, sy = float(s.height) / l->height;
        for(auto& p : pairs)
        {
          p.u = (p.u + 0.5f) * sx - 0.5f;
          p.v = (p.v + 0.5f) * sy - 0.5f;
        }
      }
      sh.recorded = std::move(pairs);
      sh.recordedVersion++;
      forceSolve = true;
    }
  }

  if(inputs.record)
  {
    const auto [cu, cv] = crosshairPixel(
        {inputs.crosshair.value.x, inputs.crosshair.value.y}, s.width, s.height);
    const auto& wp = inputs.world.value;
    sh.recorded.push_back({cu, cv, {wp.x, wp.y, wp.z}});
    sh.recordedVersion++;
  }
  if(inputs.delete_last && !sh.recorded.empty())
  {
    sh.recorded.pop_back();
    sh.recordedVersion++;
  }
  if(inputs.clear && !sh.recorded.empty())
  {
    sh.recorded.clear();
    sh.recordedVersion++;
  }
  if(forceSolve)
    m_solveWanted = true;
  m_forceSolve |= forceSolve;
}

void ManualProjectorCalibration::operator()()
{
  outputs.done.value.reset();
  auto& sh = shared();
  const bool owner = sh.solves.claim(this);
  const auto s = settings();

  if(owner)
    act(s);

  bool pairsChanged = false;
  if(m_typedDirty)
  {
    m_typedDirty = false;
    m_typed = parsePairs(inputs.points.value, &m_inputError);
    pairsChanged = true;
  }
  if(m_planesDirty)
  {
    m_planesDirty = false;
    std::string err;
    m_planes = parsePlanes(inputs.planes.value, &err);
    if(!err.empty())
      m_inputError += (m_inputError.empty() ? "" : "; ") + err;
    m_solveWanted = true;
  }
  if(m_recordedSeen != sh.recordedVersion)
  {
    m_recordedSeen = sh.recordedVersion;
    outputs.recorded.value = formatPairs(sh.recorded);
    pairsChanged = true;
  }
  if(pairsChanged)
  {
    m_all = m_typed;
    m_all.insert(m_all.end(), sh.recorded.begin(), sh.recorded.end());
    outputs.count.value = int(m_all.size());
    m_solveWanted = true;
    m_patternWanted = true;
  }
  if(!(s == m_solvedSettings))
  {
    m_solvedSettings = s;
    m_solveWanted = true;
    m_patternWanted = true;
  }

  if(owner)
  {
    if(m_solveWanted && (m_forceSolve || inputs.auto_solve.value) && !sh.solves.busy())
    {
      request(SolveJob{m_all, s, m_planes}, sh.solves.start());
      m_solveWanted = m_forceSolve = false;
    }
    else if(m_solveWanted && !inputs.auto_solve.value && !m_forceSolve)
    {
      outputs.status.value = std::to_string(m_all.size()) + " points";
      m_solveWanted = false;
    }

    if(inputs.save)
    {
      if(inputs.save_path.value.empty())
        outputs.status.value = "Failed: no Save file";
      else if(sh.saves.busy())
        outputs.status.value = "Failed: the previous save is not done";
      else
        request(
            SaveJob{inputs.save_path.value, m_all, m_solution, s, m_planes},
            sh.saves.start());
    }

    const auto ch = crosshairPixel(
        {inputs.crosshair.value.x, inputs.crosshair.value.y}, s.width, s.height);
    if(ch != m_shownCrosshair || inputs.crosshair_size.value != m_shownSize
       || inputs.level.value != m_shownLevel)
      m_patternWanted = true;
    if(m_patternWanted && !sh.patterns.busy())
    {
      m_shownCrosshair = ch;
      m_shownSize = inputs.crosshair_size.value;
      m_shownLevel = inputs.level.value;
      m_patternWanted = false;
      request(
          PatternJob{
              s.width, s.height, ch, inputs.crosshair_size.value, inputs.level.value,
              m_all, m_solution},
          sh.patterns.start());
    }
  }

  if(auto r = sh.solves.fresh(m_solvedSeen))
    publish(*r);
  if(auto r = sh.saves.fresh(m_savedSeen))
    outputs.status.value = r->status;
  if(auto p = sh.patterns.fresh(m_patternSeen))
  {
    outputs.pattern.create(p->width, p->height);
    std::copy(p->rgba.begin(), p->rgba.end(), outputs.pattern.texture.bytes);
    outputs.pattern.upload();
  }
}
}
