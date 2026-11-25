#include "StructuredLightCore.hpp"

#include <Gfx/Calibration/PlaneFitting.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Threedim
{
namespace procam = score::gfx::procam;

namespace
{
std::array<float, 3> slMul(const Eigen::Matrix4d& m, float x, float y, float z, float w)
{
  const Eigen::Vector4d r = m * Eigen::Vector4d(x, y, z, w);
  return {float(r.x()), float(r.y()), float(r.z())};
}

void throwIfCancelled(const StructuredLightCore::Job& job)
{
  if(job.cancel && job.cancel->load(std::memory_order_relaxed))
    throw std::runtime_error("cancelled");
}
}

StructuredLightCore::Result StructuredLightCore::compute(Job job)
{
  Result r;
  r.projectorIndex = job.projectorIndex;
  r.latencyMs = job.latencyMs;
  if(!job.failed.empty())
  {
    r.status = job.failed;
    return r;
  }
  if(const auto missing = job.decoder.missing(); !missing.empty())
  {
    r.status = "Failed: " + std::to_string(missing.size()) + " frames of the sequence were not captured";
    return r;
  }
  r.mode = job.mode;
  r.width = job.seq.width;
  r.height = job.seq.height;
  const int cw = job.decoder.width(), ch = job.decoder.height();
  r.camWidth = cw;
  r.camHeight = ch;

  auto dec = job.decoder.decode(job.minSignal, job.minBits);
  throwIfCancelled(job);
  r.decoded.assign(std::size_t(cw) * ch * 4, 0);
  r.seq = job.seq;
  int decodedCount = 0, phaseCount = 0;
  for(std::size_t i = 0; i < dec.size(); i++)
  {
    if(!dec[i].valid)
      continue;
    decodedCount++;
    phaseCount += dec[i].phaseU && dec[i].phaseV;
    uint8_t* p = r.decoded.data() + i * 4;
    p[0] = uint8_t(std::clamp(dec[i].u / r.width, 0.f, 1.f) * 255.f);
    p[1] = uint8_t(std::clamp(dec[i].v / r.height, 0.f, 1.f) * 255.f);
    p[2] = uint8_t(std::clamp(dec[i].confidence, 0.f, 1.f) * 255.f);
    p[3] = 255;
  }
  if(decodedCount == 0)
  {
    r.status = "Failed: no camera pixel decoded (check the pattern level, exposure and "
               "settle time)";
    return r;
  }

  r.cameraIntrinsics = job.cameraIntrinsics;
  r.lens = job.lens;
  const Eigen::Matrix4d M = procam::fromColumnMajor(job.worldFromCamera);
  sl::SparseMap map;
  map.reset(r.width, r.height);

  if(job.mode == DepthCamera)
  {
    std::vector<procam::Vec3> X;
    std::vector<procam::Vec2> x;
    std::vector<float> conf;
    std::vector<uint8_t> precise, edge;
    for(std::size_t i = 0; i < dec.size(); i++)
    {
      if(!dec[i].valid || job.positionFrames == 0 || job.positionCount[i] < job.positionFrames
         || job.zMax[i] - job.zMin[i] > 0.03f)
        continue;
      const float n = float(job.positionCount[i]);
      const auto w = slMul(
          M, job.positions[i * 3] / n, job.positions[i * 3 + 1] / n,
          job.positions[i * 3 + 2] / n, 1.f);
      X.emplace_back(w[0], w[1], w[2]);
      x.emplace_back(dec[i].u, dec[i].v);
      conf.push_back(dec[i].confidence);
      precise.push_back(dec[i].phaseU && dec[i].phaseV);
      edge.push_back(dec[i].rim || dec[i].saturated);
      map.keepBest(int(std::lround(dec[i].u)), int(std::lround(dec[i].v)), w, dec[i].confidence);
    }
    if(X.size() < 6)
    {
      r.status = "Failed: " + std::to_string(X.size())
                 + " decoded pixels have a depth; no Positions input, or no depth "
                   "under the lit area";
      return r;
    }

    std::vector<std::size_t> use;
    for(std::size_t i = 0; i < X.size(); i++)
      if(precise[i] && !edge[i])
        use.push_back(i);
    bool phaseDriven = true;
    if(use.size() < std::max<std::size_t>(500, X.size() / 4))
    {
      phaseDriven = false;
      use.clear();
      for(std::size_t i = 0; i < X.size(); i++)
        if(!edge[i])
          use.push_back(i);
    }
    if(use.size() < 6)
    {
      use.resize(X.size());
      for(std::size_t i = 0; i < X.size(); i++)
        use[i] = i;
    }
    std::vector<procam::Vec3> Xs;
    std::vector<procam::Vec2> xs;
    const std::size_t keep = std::min<std::size_t>(use.size(), 6000);
    for(std::size_t k = 0; k < keep; k++)
    {
      const std::size_t i = use[keep == use.size() ? k : (k * use.size()) / keep];
      Xs.push_back(X[i]);
      xs.push_back(x[i]);
    }
    procam::Options opt;
    opt.ransac = true;
    opt.ransacThreshold = job.ransac > 0.f ? job.ransac : (phaseDriven ? 3.f : 12.f);
    procam::Model known;
    known.fx = job.lockedK[0];
    known.fy = job.lockedK[1];
    known.cx = job.lockedK[2];
    known.cy = job.lockedK[3];
    auto run = [&] {
      return job.lockK ? procam::solvePose(Xs, xs, known, opt) : procam::solve(Xs, xs, opt);
    };
    auto sol = run();
    if(job.ransac <= 0.f && phaseDriven
       && (!sol.ok || sol.inliers < int(Xs.size() / 2)))
    {
      opt.ransacThreshold = 12.f;
      sol = run();
    }
    r.intrinsicsLocked = job.lockK;
    r.intrinsicsMode = job.intrinsicsMode;
    if(sol.ok)
    {
      const auto& m = sol.model;
      r.intrinsics = {float(m.fx), float(m.fy), float(m.cx), float(m.cy)};
      r.pose = procam::toColumnMajor(m.worldFromLocal());
      r.rms = float(sol.rms);
    }
    r.ransac = float(opt.ransacThreshold);
    const auto raw = sl::pushPullFill(map, job.holeRadius, job.depthJump, false);
    const int planeRadius = std::max(
        job.holeRadius,
        int(0.15 * std::sqrt(double(r.width) * r.width + double(r.height) * r.height)));
    r.status = sol.ok ? "" : ("Projector solve failed: " + sol.error + "; ");
    r.xyz = raw;
    if(sol.ok && job.surface != RawMeasured)
    {
      const auto& m = sol.model;
      const Eigen::Matrix3d Rt = m.R.transpose();
      const Eigen::Vector3d o = m.position();
      auto ray = [&](int u, int v) -> Eigen::Vector3d {
        return Rt * Eigen::Vector3d((u - m.cx) / m.fx, (v - m.cy) / m.fy, 1.);
      };

      if(job.surface == Planes)
      {
        std::vector<Eigen::Vector3d> pts;
        const std::size_t keep = std::min<std::size_t>(X.size(), 20000);
        pts.reserve(keep);
        for(std::size_t k = 0; k < keep; k++)
          pts.push_back(X[keep == X.size() ? k : (k * X.size()) / keep]);
        score::gfx::planes::Options popt;
        popt.maxPlanes = std::max(1, job.maxPlanes);
        popt.threshold = job.planeThreshold;
        popt.iterations = 600;
        popt.minInliers = std::max<int>(50, int(pts.size() / 20));
        const auto found = score::gfx::planes::fitPlanes(pts, o, popt);
        for(auto& pl : found)
          r.planes.push_back(
              {float(pl.n.x()), float(pl.n.y()), float(pl.n.z()), float(pl.d)});

        const auto mask = found.empty()
                              ? std::vector<float>{}
                              : sl::pushPullFill(map, planeRadius, job.depthJump, false);
        if(found.empty())
          r.status += "no plane found, raw measured map; ";
        else
          for(int v = 0; v < r.height; v++)
            for(int u = 0; u < r.width; u++)
            {
              float* out = r.xyz.data() + (std::size_t(v) * r.width + u) * 4;
              if(mask[(std::size_t(v) * r.width + u) * 4 + 3] <= 0.f)
              {
                out[0] = out[1] = out[2] = out[3] = 0.f;
                continue;
              }
              const Eigen::Vector3d d = ray(u, v);
              double best = std::numeric_limits<double>::infinity();
              for(auto& pl : found)
              {
                const double den = pl.n.dot(d);
                if(std::abs(den) < 1e-12)
                  continue;
                const double t = -(pl.n.dot(o) + pl.d) / den;
                if(t > 0. && t < best)
                  best = t;
              }
              if(!std::isfinite(best))
              {
                out[0] = out[1] = out[2] = out[3] = 0.f;
                continue;
              }
              const Eigen::Vector3d p = o + best * d;
              out[0] = float(p.x());
              out[1] = float(p.y());
              out[2] = float(p.z());
              out[3] = 1.f;
            }
      }
      else
      {
        sl::SparseMap depth;
        depth.reset(r.width, r.height);
        for(std::size_t i = 0; i < X.size(); i++)
        {
          const float z = float((m.R * X[i] + m.t).z());
          if(z > 0.f)
            depth.splat(float(x[i].x()), float(x[i].y()), {z, 0.f, 0.f}, conf[i]);
        }
        const auto filled = sl::pushPullFill(depth, job.holeRadius, job.depthJump, true);
        for(int v = 0; v < r.height; v++)
          for(int u = 0; u < r.width; u++)
          {
            const std::size_t i = std::size_t(v) * r.width + u;
            float* out = r.xyz.data() + i * 4;
            if(filled[i * 4 + 3] <= 0.f)
            {
              out[0] = out[1] = out[2] = out[3] = 0.f;
              continue;
            }
            const Eigen::Vector3d p = o + double(filled[i * 4]) * ray(u, v);
            out[0] = float(p.x());
            out[1] = float(p.y());
            out[2] = float(p.z());
            out[3] = 1.f;
          }
      }
    }
  }
  else
  {
    const auto& k = job.cameraIntrinsics;
    if(!(k[0] > 0.f && k[1] > 0.f) || k[2] < 0.f || k[3] < 0.f || k[2] > float(cw)
       || k[3] > float(ch))
    {
      char buf[200];
      std::snprintf(
          buf, sizeof(buf),
          "Failed: the camera intrinsics (fx %.1f fy %.1f cx %.1f cy %.1f) are not those "
          "of a %d x %d camera",
          k[0], k[1], k[2], k[3], cw, ch);
      r.status = buf;
      return r;
    }
    const std::array<double, 4> K{k[0], k[1], k[2], k[3]};
    const Eigen::Matrix3d R = M.topLeftCorner<3, 3>();
    for(std::size_t i = 0; i < dec.size(); i++)
    {
      if(!dec[i].valid)
        continue;
      const int px = int(i % cw), py = int(i / cw);
      const auto u = job.lens.unproject(K, px, py);
      Eigen::Vector3d d(u[0], u[1], u[2]);
      d = (R * d).normalized();
      map.splat(dec[i].u, dec[i].v, {float(d.x()), float(d.y()), float(d.z())}, 1.f);
    }
    r.xyz = sl::pushPullFill(map, job.holeRadius, 0.f, true);
    for(std::size_t i = 0; i < r.xyz.size(); i += 4)
    {
      if(r.xyz[i + 3] <= 0.f)
        continue;
      const float n = std::sqrt(
          r.xyz[i] * r.xyz[i] + r.xyz[i + 1] * r.xyz[i + 1] + r.xyz[i + 2] * r.xyz[i + 2]);
      if(n > 1e-9f)
        for(int c = 0; c < 3; c++)
          r.xyz[i + c] /= n;
    }
  }

  r.cameraGamma = job.cameraGamma;
  r.crf = job.crf;
  const float camGamma = job.cameraGamma > 0.f ? job.cameraGamma : 1.f;
  auto lin = [&](int c, float v) -> float {
    if(job.crf)
      return float(job.crf->linear(c, v));
    return camGamma == 1.f ? v : std::pow(std::max(0.f, v), camGamma);
  };
  const std::size_t camN = std::size_t(cw) * ch;
  const bool rgb = job.whiteRGB.size() == camN * 3 && job.blackRGB.size() == camN * 3;
  {
    const auto& wl = job.decoder.white();
    const auto& bl = job.decoder.black();
    if(rgb || (wl.size() == camN && bl.size() == camN))
    {
      sl::SparseMap white, black;
      white.reset(r.width, r.height);
      black.reset(r.width, r.height);
      for(std::size_t i = 0; i < dec.size(); i++)
      {
        if(!dec[i].valid || dec[i].rim || dec[i].saturated)
          continue;
        std::array<float, 3> w, b;
        for(int c = 0; c < 3; c++)
        {
          b[c] = rgb ? lin(c, job.blackRGB[i * 3 + c]) : bl[i];
          w[c] = std::max(0.f, (rgb ? lin(c, job.whiteRGB[i * 3 + c]) : wl[i]) - b[c]);
        }
        white.splat(dec[i].u, dec[i].v, w, 1.f);
        black.splat(dec[i].u, dec[i].v, b, 1.f);
      }
      const int radius = std::max(4, job.holeRadius);
      r.lum = sl::pushPullFill(white, radius, 0.f, true);
      r.black = sl::pushPullFill(black, radius, 0.f, true);
      for(std::size_t i = 3; i < r.lum.size() && i < r.xyz.size(); i += 4)
        if(r.xyz[i] <= 0.f)
          r.lum[i] = r.black[i] = 0.f;
    }
  }

  std::string photoNote;
  computePhotometric(job, dec, r, photoNote);

  throwIfCancelled(job);
  std::size_t covered = 0;
  for(std::size_t i = 3; i < r.xyz.size(); i += 4)
    covered += r.xyz[i] > 0.f;
  r.coverage = 100.f * float(covered) / float(std::max<std::size_t>(1, r.xyz.size() / 4));
  r.ok = true;
  char buf[160];
  std::snprintf(
      buf, sizeof(buf), "Done: %.1f%% of the camera decoded, %.1f%% of the projector covered",
      100.f * decodedCount / float(std::max(1, cw * ch)), r.coverage);
  r.status += buf;
  if(job.mode == DepthCamera && r.rms > 0.f)
  {
    if(job.lockK)
      std::snprintf(
          buf, sizeof(buf),
          ", intrinsics locked (fx %.1f fy %.1f cx %.1f cy %.1f), pose RMS %.2f px (RANSAC "
          "%.1f px)",
          job.lockedK[0], job.lockedK[1], job.lockedK[2], job.lockedK[3], r.rms, r.ransac);
    else
      std::snprintf(buf, sizeof(buf), ", RMS %.2f px (RANSAC %.1f px)", r.rms, r.ransac);
    r.status += buf;
  }
  if(!job.kNote.empty())
    r.status += "; " + job.kNote;
  r.phaseValid = 100.f * phaseCount / float(std::max(1, decodedCount));
  if(job.seq.phaseSteps > 0)
  {
    std::snprintf(
        buf, sizeof(buf), ", phase valid on %.1f%% of decoded pixels (%d steps, period %d px)",
        r.phaseValid, job.seq.phaseSteps, job.seq.period);
    r.status += buf;
  }
  std::snprintf(
      buf, sizeof(buf), "; %d patterns: Gray %d+%d bits", job.seq.count(),
      job.seq.projectedX(), job.seq.projectedY());
  r.status += buf;
  if(job.crf)
    r.status += ", camera response linearised";
  else if(job.crfRejected)
    r.status += ", camera response rejected (flat over the mid codes: did the camera "
                "apply the bracket's exposures?), camera gamma used instead";
  r.status += photoNote;
  if(job.latencyMs > 0.)
  {
    std::snprintf(buf, sizeof(buf), ", camera latency %.0f ms", job.latencyMs);
    r.status += buf;
  }
  if(job.keepPixels)
    r.pixels = std::move(dec);
  return r;
}



void StructuredLightCore::commonCompensation(std::vector<Result>& results)
{
  float common = 0.f;
  for(auto& r : results)
    if(r.compScale > 0.f)
      common = common > 0.f ? std::min(common, r.compScale) : r.compScale;
  for(auto& r : results)
    if(r.compScale > 0.f)
    {
      r.compScale = common;
      if(results.size() > 1)
        r.compGradient = 0.f;
    }
}

QString StructuredLightCore::projectorPath(const QString& path, int index)
{
  return Gfx::calibrationSetName(Gfx::calibrationBase(path), index);
}

bool StructuredLightCore::saveFile(const QString& path, const Result& r, QString* error)
{
  if(!r.ok || r.xyz.empty() || r.xyz.size() != std::size_t(r.width) * r.height * 4)
  {
    if(error)
      *error = QStringLiteral("no result to save");
    return false;
  }
  const std::size_t n = std::size_t(r.width) * r.height;
  auto rgb = [&](const std::vector<float>& rgba) {
    std::vector<float> out;
    if(rgba.size() != n * 4)
      return out;
    out.resize(n * 3);
    for(std::size_t i = 0; i < n; i++)
      for(int c = 0; c < 3; c++)
        out[i * 3 + c] = rgba[i * 4 + c];
    return out;
  };
  CalibrationFile c;
  c.base = Gfx::calibrationBase(path);
  c.width = r.width;
  c.height = r.height;
  c.xyz = r.xyz;
  c.viewerMode = r.mode == ViewerCamera;
  c.intrinsics = r.intrinsics;
  c.hasPose = r.intrinsics[0] > 0.f;
  c.pose = r.pose;
  c.lum = rgb(r.lum);
  c.black = rgb(r.black);
  if(r.cameraIntrinsics[0] > 0.f)
    c.camera = CalibrationCamera{r.cameraIntrinsics, r.camWidth, r.camHeight, r.lens};
  c.planes = r.planes;
  c.projectorIndex = r.projectorIndex;
  c.screen = r.screen;
  c.json["mode"] = c.viewerMode ? "viewer" : "depth";
  c.json["rms"] = r.rms;
  c.json["coverage"] = r.coverage;
  c.json["intrinsicsMode"] = r.intrinsicsMode == LockIntrinsics  ? "lock"
                             : r.intrinsicsMode == LockToLoaded ? "lockToLoaded"
                                                                : "solve";
  c.json["intrinsicsLocked"] = r.intrinsicsLocked;
  QJsonObject seq;
  seq["patterns"] = r.seq.count();
  seq["grayBitsX"] = r.seq.projectedX();
  seq["grayBitsY"] = r.seq.projectedY();
  seq["grayBitsSkipped"] = r.seq.skip;
  seq["phaseSteps"] = r.seq.phaseSteps;
  seq["phasePeriod"] = r.seq.period;
  seq["projectorGamma"] = r.seq.gamma;
  seq["phaseValidPercent"] = r.phaseValid;
  if(r.latencyMs > 0.)
    seq["cameraLatencyMs"] = r.latencyMs;
  c.json["sequence"] = seq;

  auto w = calibrationWrite(c);
  w.keepListed.clear();
  if(!c.hasPose)
    w.removeKeys = {"intrinsics", "worldFromProjector"};
  QJsonObject photometric;
  if(!c.lum.empty() && !c.black.empty())
  {
    photometric["luminance"] = QFileInfo{c.base + ".lum.pfm"}.fileName();
    photometric["black"] = QFileInfo{c.base + ".black.pfm"}.fileName();
    photometric["cameraGamma"] = r.cameraGamma;
    photometric["note"] = "linear camera RGB per projector pixel: white - black, and "
                          "black (ambient included)";
  }
  addPhotometricFiles(r, w, photometric);
  if(!photometric.isEmpty())
    w.fields["photometric"] = photometric;
  return writeCalibration(w, error);
}

StructuredLightCore::Result StructuredLightCore::loadFile(const QString& path)
{
  Result r;
  const auto c = loadCalibration(path);
  if(!c.ok())
  {
    r.status = "Failed: " + c.error.toStdString();
    return r;
  }
  r.width = c.width;
  r.height = c.height;
  r.xyz = c.xyz;
  r.mode = c.viewerMode ? ViewerCamera : DepthCamera;
  if(c.hasPose)
  {
    r.intrinsics = c.intrinsics;
    r.pose = c.pose;
  }
  r.rms = float(c.json.value("rms").toDouble());
  const auto km = c.json.value("intrinsicsMode").toString();
  r.intrinsicsMode = km == "lock"           ? LockIntrinsics
                     : km == "lockToLoaded" ? LockToLoaded
                                            : SolveIntrinsics;
  r.intrinsicsLocked = c.json.value("intrinsicsLocked").toBool();
  if(c.camera)
  {
    r.cameraIntrinsics = c.camera->intrinsics;
    r.camWidth = c.camera->width;
    r.camHeight = c.camera->height;
    r.lens = c.camera->lens;
  }
  r.planes = c.planes;
  const std::size_t n = std::size_t(r.width) * r.height;
  auto rgba = [&](const std::vector<float>& rgb) {
    std::vector<float> out;
    if(rgb.size() != n * 3)
      return out;
    out.resize(n * 4);
    for(std::size_t i = 0; i < n; i++)
    {
      for(int k = 0; k < 3; k++)
        out[i * 4 + k] = rgb[i * 3 + k];
      out[i * 4 + 3] = r.xyz[i * 4 + 3];
    }
    return out;
  };
  r.lum = rgba(c.lum);
  r.black = rgba(c.black);
  r.cameraGamma
      = float(c.json.value("photometric").toObject().value("cameraGamma").toDouble(1.));
  if(c.projectorIndex >= 0)
    r.projectorIndex = c.projectorIndex;
  else
  {
    static const QRegularExpression index{QStringLiteral("_p([0-9]+)$")};
    if(const auto m = index.match(c.base); m.hasMatch())
      r.projectorIndex = m.captured(1).toInt();
  }
  r.screen = c.screen;
  std::string note;
  loadPhotometricFiles(c.base, r, note);
  std::size_t covered = 0;
  for(std::size_t i = 3; i < r.xyz.size(); i += 4)
    covered += r.xyz[i] > 0.f;
  r.coverage = 100.f * float(covered) / float(std::max<std::size_t>(1, n));
  r.ok = true;
  r.fromFile = true;
  r.status = "Loaded " + QString(c.base + ".json").toStdString() + note;
  return r;
}
}
