#include "StructuredLightCalibration.hpp"

#include <Gfx/Calibration/PlaneFitting.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>

namespace Threedim
{
namespace procam = score::gfx::procam;

namespace
{
std::string slModeName(int mode)
{
  return mode == StructuredLightCalibration::ViewerCamera ? "viewer" : "depth";
}

std::array<float, 3> slMul(const Eigen::Matrix4d& m, float x, float y, float z, float w)
{
  const Eigen::Vector4d r = m * Eigen::Vector4d(x, y, z, w);
  return {float(r.x()), float(r.y()), float(r.z())};
}
}

StructuredLightCalibration::Result StructuredLightCalibration::compute(Job job)
{
  Result r;
  r.mode = job.mode;
  r.width = job.seq.width;
  r.height = job.seq.height;
  const int cw = job.decoder.width(), ch = job.decoder.height();
  r.camWidth = cw;
  r.camHeight = ch;

  const auto dec = job.decoder.decode(job.minSignal, job.minBits);
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
    r.status = job.whiteMax <= 1e-6f
                   ? "Failed: the camera image was black during the white pattern: the "
                     "camera delivered no frame (is it streaming?)"
                   : "Failed: no camera pixel decoded (check the pattern level, exposure "
                     "and settle time)";
    return r;
  }

  const Eigen::Matrix4d M = procam::fromColumnMajor(job.worldFromCamera);
  sl::SparseMap map;
  map.reset(r.width, r.height);

  if(job.mode == DepthCamera)
  {
    std::vector<procam::Vec3> X;
    std::vector<procam::Vec2> x;
    std::vector<float> conf;
    std::vector<uint8_t> precise, rim;
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
      rim.push_back(dec[i].rim);
      map.keepBest(int(std::lround(dec[i].u)), int(std::lround(dec[i].v)), w, dec[i].confidence);
    }
    if(X.size() < 6)
    {
      r.status = "Failed: " + std::to_string(X.size())
                 + " decoded pixels have a depth; no Positions input, or no depth "
                   "under the lit area";
      return r;
    }

    // With phase shift, the Gray-only pixels are an order of magnitude less
    // precise: the solver uses the phase-decoded ones when there are enough.
    // Pixels on the rim of the lit area are biased by the blur and kept out
    // of the solver, which they would pull, but not out of the maps.
    std::vector<std::size_t> use;
    for(std::size_t i = 0; i < X.size(); i++)
      if(precise[i] && !rim[i])
        use.push_back(i);
    bool phaseDriven = true;
    if(use.size() < std::max<std::size_t>(500, X.size() / 4))
    {
      phaseDriven = false;
      use.clear();
      for(std::size_t i = 0; i < X.size(); i++)
        if(!rim[i])
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
    // A tight automatic threshold on a phase that is off by more (an
    // uncompensated gamma with 3 steps) keeps a minority of the points and can
    // lock onto a wrong model: the Gray-code threshold is then used.
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
    const Eigen::Matrix3d R = M.topLeftCorner<3, 3>();
    for(std::size_t i = 0; i < dec.size(); i++)
    {
      if(!dec[i].valid)
        continue;
      const int px = int(i % cw), py = int(i / cw);
      Eigen::Vector3d d((px - k[2]) / k[0], (py - k[3]) / k[1], 1.);
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
  r.projectorIndex = job.projectorIndex;
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
        if(!dec[i].valid || dec[i].rim)
          continue;
        std::array<float, 3> w, b;
        for(int c = 0; c < 3; c++)
        {
          const float wv = rgb ? job.whiteRGB[i * 3 + c] : wl[i];
          const float bv = rgb ? job.blackRGB[i * 3 + c] : bl[i];
          b[c] = lin(c, bv);
          w[c] = std::max(0.f, lin(c, wv) - b[c]);
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

  // Camera pixels usable for the uniform frames: decoded, off the rim, and not
  // clipped in the white frame.
  auto usable = [&](std::size_t i) {
    if(i >= dec.size() || !dec[i].valid || dec[i].rim)
      return false;
    if(rgb)
      for(int c = 0; c < 3; c++)
        if(job.whiteRGB[i * 3 + c] >= 0.985f)
          return false;
    return true;
  };
  const int S = std::max(1, job.extraStride);
  const int sw = (cw + S - 1) / S, shh = (ch + S - 1) / S;

  std::string photoNote;
  if(rgb)
  {
    // Projector response: the mean linear colour over the usable pixels of
    // each level, projected on the channel's full-level colour.
    std::array<std::vector<std::pair<double, std::array<double, 3>>>, 4> ramp;
    std::array<double, 3> blackMean{};
    int nb = 0;
    std::vector<std::size_t> pix;
    for(int y = 0; y < shh; y++)
      for(int x = 0; x < sw; x++)
      {
        const std::size_t i = std::size_t(y * S) * cw + x * S;
        if(usable(i))
          pix.push_back(std::size_t(y) * sw + x);
      }
    for(std::size_t q : pix)
    {
      const std::size_t i = std::size_t(q / sw) * S * cw + (q % sw) * S;
      for(int c = 0; c < 3; c++)
        blackMean[c] += lin(c, job.blackRGB[i * 3 + c]);
      nb++;
    }
    auto meanColour = [&](const Extra& e) {
      std::array<double, 3> m{};
      const bool full = e.rgbCapture.size() == camN * 3;
      for(std::size_t q : pix)
      {
        const std::size_t ci = std::size_t(q / sw) * S * cw + (q % sw) * S;
        const std::size_t k = full ? ci : q;
        if(k * 3 + 2 >= e.rgbCapture.size())
          continue;
        for(int c = 0; c < 3; c++)
          m[c] += lin(c, e.rgbCapture[k * 3 + c]);
      }
      for(int c = 0; c < 3; c++)
        m[c] = m[c] / double(pix.size()) - blackMean[c] / std::max(1, nb);
      return m;
    };
    for(auto& e : job.extras)
    {
      if(e.kind != RampFrame || pix.empty())
        continue;
      const auto m = meanColour(e);
      const float lv = e.channel < 3 ? e.rgb[e.channel] : e.rgb[0];
      ramp[e.channel].push_back({std::round(lv * 255.) / 255., m});
    }
    if(!ramp[0].empty() && !ramp[1].empty() && !ramp[2].empty())
    {
      score::gfx::ProjectorResponseFile pr;
      for(auto& e : job.extras)
        if((e.kind == RampFrame || e.kind == MixFrame || e.kind == GridFrame) && !pix.empty())
          pr.samples.push_back({e.rgb, meanColour(e)});
      pr.linearisation = job.crf ? QStringLiteral("camera response (Debevec & Malik)")
                                 : QStringLiteral("camera gamma %1").arg(camGamma);
      for(int c = 0; c < 4; c++)
      {
        auto& rr = ramp[c];
        if(rr.empty())
          continue;
        std::sort(rr.begin(), rr.end(), [](auto& a, auto& b) { return a.first < b.first; });
        const auto dir = rr.back().second;
        std::vector<double> x, m;
        for(auto& [lv, col] : rr)
        {
          x.push_back(lv);
          m.push_back(std::max(0., score::gfx::photometric::projectOn(col, dir)));
        }
        pr.channels[c] = score::gfx::photometric::fitChannelResponse(x, m);
      }
      if(!ramp[3].empty() && ramp[3].size() == ramp[0].size())
      {
        const auto& top = ramp[3].back().second;
        const double tn = std::sqrt(top[0] * top[0] + top[1] * top[1] + top[2] * top[2]);
        double err = 0.;
        for(std::size_t k = 0; k < ramp[3].size(); k++)
        {
          double d2 = 0.;
          for(int c = 0; c < 3; c++)
          {
            const double sum
                = ramp[0][k].second[c] + ramp[1][k].second[c] + ramp[2][k].second[c];
            d2 += (ramp[3][k].second[c] - sum) * (ramp[3][k].second[c] - sum);
          }
          err = std::max(err, std::sqrt(d2) / std::max(1e-12, tn));
        }
        pr.additivityError = err;
      }
      Eigen::Matrix3d Vm;
      for(int p = 0; p < 3; p++)
        for(int c = 0; c < 3; c++)
          Vm(c, p) = ramp[p].back().second[c];
      Eigen::FullPivLU<Eigen::Matrix3d> lu(Vm);
      if(lu.isInvertible())
      {
        const Eigen::Matrix3d Vi = lu.inverse();
        auto linear = [&](const std::array<float, 3>& code) {
          return Eigen::Vector3d(
              pr.channels[0](code[0]), pr.channels[1](code[1]), pr.channels[2](code[2]));
        };
        const int N = job.mixLevels;
        if(N >= 2)
        {
          auto key = [](const std::array<float, 3>& code) {
            return std::array<int, 3>{
                int(std::lround(code[0] * 255.f)), int(std::lround(code[1] * 255.f)),
                int(std::lround(code[2] * 255.f))};
          };
          std::map<std::array<int, 3>, const Extra*> byCode;
          for(auto& e : job.extras)
            if(e.kind == RampFrame || e.kind == GridFrame)
              byCode.emplace(key(e.rgb), &e);
          score::gfx::photometric::ColourMixing mx;
          for(int c = 0; c < 3; c++)
            for(int k = 0; k < N; k++)
              mx.axis[c].push_back(pr.channels[c](double(k) / (N - 1)));
          bool complete = true;
          for(int b = 0; b < N && complete; b++)
            for(int g = 0; g < N && complete; g++)
              for(int x = 0; x < N && complete; x++)
              {
                const std::array<float, 3> code{
                    float(x) / (N - 1), float(g) / (N - 1), float(b) / (N - 1)};
                if(x == 0 && g == 0 && b == 0)
                {
                  mx.shown.push_back({0., 0., 0.});
                  continue;
                }
                const auto it = byCode.find(key(code));
                if(it == byCode.end())
                {
                  complete = false;
                  break;
                }
                const auto m = meanColour(*it->second);
                const Eigen::Vector3d e = Vi * Eigen::Vector3d(m[0], m[1], m[2]);
                mx.shown.push_back({e[0], e[1], e[2]});
              }
          if(complete && mx.valid())
            pr.mixing = std::move(mx);
        }
        double eAdd = 0., eModel = 0.;
        int mixes = 0;
        for(auto& e : job.extras)
        {
          if(e.kind != MixFrame || pix.empty())
            continue;
          const auto m = meanColour(e);
          const Eigen::Vector3d C(m[0], m[1], m[2]);
          const double n = C.norm();
          if(n <= 0.)
            continue;
          const Eigen::Vector3d l = linear(e.rgb);
          eAdd += (Vm * l - C).norm() / n;
          if(pr.mixing.valid())
          {
            const auto y = pr.mixing.forward({l[0], l[1], l[2]});
            eModel += (Vm * Eigen::Vector3d(y[0], y[1], y[2]) - C).norm() / n;
          }
          mixes++;
        }
        if(mixes > 0)
        {
          pr.mixErrorAdditive = eAdd / mixes;
          if(pr.mixing.valid())
            pr.mixErrorModel = eModel / mixes;
        }
      }
      char nb2[160];
      std::snprintf(
          nb2, sizeof(nb2), ", projector gamma %.2f %.2f %.2f", pr.channels[0].gamma,
          pr.channels[1].gamma, pr.channels[2].gamma);
      photoNote += nb2;
      if(pr.additivityError >= 0.)
      {
        std::snprintf(nb2, sizeof(nb2), " (additivity error %.1f%%)", 100. * pr.additivityError);
        photoNote += nb2;
      }
      if(pr.mixing.valid())
      {
        const auto x = pr.mixing.forward({1., 1., 1.});
        std::snprintf(
            nb2, sizeof(nb2), ", white shown at %.2f %.2f %.2f of the primaries' sum", x[0],
            x[1], x[2]);
        photoNote += nb2;
      }
      if(pr.mixErrorAdditive >= 0.)
      {
        std::snprintf(nb2, sizeof(nb2), ", mixed colours error %.1f%% additive", 100. * pr.mixErrorAdditive);
        photoNote += nb2;
        if(pr.mixErrorModel >= 0.)
        {
          std::snprintf(nb2, sizeof(nb2), ", %.1f%% with the measured mixing", 100. * pr.mixErrorModel);
          photoNote += nb2;
        }
      }
      r.response = std::move(pr);
    }

    // Surface compensation: V per camera pixel from the full primaries.
    const Extra* prim[3]{};
    for(auto& e : job.extras)
      if(e.full && e.channel < 3 && e.rgb[e.channel] >= 1.f && e.rgbCapture.size() == camN * 3)
        prim[e.channel] = &e;
    if(prim[0] && prim[1] && prim[2])
    {
      std::array<sl::SparseMap, 4> cols;
      for(auto& m : cols)
        m.reset(r.width, r.height);
      for(std::size_t i = 0; i < dec.size(); i++)
      {
        if(!usable(i))
          continue;
        std::array<float, 3> b;
        for(int c = 0; c < 3; c++)
          b[c] = lin(c, job.blackRGB[i * 3 + c]);
        for(int p = 0; p < 3; p++)
        {
          std::array<float, 3> col;
          for(int c = 0; c < 3; c++)
            col[c] = lin(c, prim[p]->rgbCapture[i * 3 + c]) - b[c];
          cols[p].splat(dec[i].u, dec[i].v, col, 1.f);
        }
        cols[3].splat(dec[i].u, dec[i].v, b, 1.f);
      }
      const int radius = std::max(4, job.holeRadius);
      std::array<std::vector<float>, 4> filled;
      for(int p = 0; p < 4; p++)
        filled[p] = sl::pushPullFill(cols[p], radius, 0.f, true);
      const std::size_t n = std::size_t(r.width) * r.height;
      r.compInverse.assign(n * 9, 0.f);
      r.compBlack.assign(n * 3, 0.f);
      r.compValid.assign(n, 0);
      const score::gfx::photometric::ColourMixing noMixing;
      const auto& mixing = r.response ? r.response->mixing : noMixing;
      const auto mixInv = score::gfx::photometric::MixingInverse::build(mixing);
      const auto whiteTop = mixing.valid() ? mixing.forward({1., 1., 1.})
                                           : std::array<double, 3>{1., 1., 1.};
      std::array<double, 3> target{1., 1., 1.};
      {
        std::array<std::vector<float>, 3> chroma;
        for(std::size_t i = 0; i < n; i += 7)
        {
          if(filled[0][i * 4 + 3] <= 0.f)
            continue;
          std::array<double, 3> w{};
          for(int p = 0; p < 3; p++)
            for(int c = 0; c < 3; c++)
              w[c] += filled[p][i * 4 + c] * whiteTop[p];
          const double y = 0.2126 * w[0] + 0.7152 * w[1] + 0.0722 * w[2];
          if(y <= 1e-9)
            continue;
          for(int c = 0; c < 3; c++)
            chroma[c].push_back(float(w[c] / y));
        }
        if(chroma[0].size() >= 16)
        {
          for(int c = 0; c < 3; c++)
          {
            auto& v = chroma[c];
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            target[c] = v[v.size() / 2];
          }
          const double m = std::max({target[0], target[1], target[2]});
          for(auto& t : target)
            t /= m;
        }
      }
      std::vector<float> scales;
      for(std::size_t i = 0; i < n; i++)
      {
        float* Mi = r.compInverse.data() + i * 9;
        Mi[0] = Mi[4] = Mi[8] = 1.f;
        if(filled[0][i * 4 + 3] <= 0.f || (i * 4 + 3 < r.xyz.size() && r.xyz[i * 4 + 3] <= 0.f))
          continue;
        Eigen::Matrix3d V;
        for(int p = 0; p < 3; p++)
          for(int c = 0; c < 3; c++)
            V(c, p) = filled[p][i * 4 + c];
        Eigen::Matrix3d M;
        if(!score::gfx::photometric::compensationMatrix(V, target, M))
          continue;
        for(int a = 0; a < 3; a++)
          for(int bb = 0; bb < 3; bb++)
            Mi[a * 3 + bb] = float(M(a, bb));
        for(int c = 0; c < 3; c++)
          r.compBlack[i * 3 + c] = filled[3][i * 4 + c];
        r.compValid[i] = 1;
        scales.push_back(float(score::gfx::photometric::maxWhiteScale(M, mixing, mixInv)));
      }
      if(!scales.empty())
      {
        std::array<sl::SparseMap, 4> known;
        for(auto& m : known)
          m.reset(r.width, r.height);
        bool holes = false;
        for(std::size_t i = 0; i < n; i++)
        {
          if(!r.compValid[i])
          {
            holes = true;
            continue;
          }
          const int u = int(i % r.width), v = int(i / r.width);
          const float* Mi = r.compInverse.data() + i * 9;
          for(int a = 0; a < 3; a++)
            known[a].accumulate(u, v, {Mi[a * 3], Mi[a * 3 + 1], Mi[a * 3 + 2]});
          known[3].accumulate(
              u, v, {r.compBlack[i * 3], r.compBlack[i * 3 + 1], r.compBlack[i * 3 + 2]});
        }
        if(holes)
        {
          const int reach = std::max(r.width, r.height);
          std::array<std::vector<float>, 4> f;
          for(int a = 0; a < 4; a++)
            f[a] = sl::pushPullFill(known[a], reach, 0.f, true);
          for(std::size_t i = 0; i < n; i++)
          {
            if(r.compValid[i] || f[0][i * 4 + 3] <= 0.f)
              continue;
            float* Mi = r.compInverse.data() + i * 9;
            for(int a = 0; a < 3; a++)
              for(int b = 0; b < 3; b++)
                Mi[a * 3 + b] = f[a][i * 4 + b];
            for(int c = 0; c < 3; c++)
              r.compBlack[i * 3 + c] = f[3][i * 4 + c];
            r.compValid[i] = 1;
          }
        }
      }
      if(!scales.empty())
      {
        const float g = std::clamp(job.compGamut, 0.f, 1.f);
        const std::size_t q = std::min(
            scales.size() - 1, std::size_t(std::floor((1.f - g) * float(scales.size() - 1))));
        std::nth_element(scales.begin(), scales.begin() + q, scales.end());
        r.compScale = scales[q];
        r.compGamut = job.compGamut;
        r.compStrength = job.compStrength;
        r.compGradient = job.compGradient;
        r.compBlackOn = job.compBlack;
        char nb2[160];
        std::snprintf(
            nb2, sizeof(nb2), ", compensation white %.3f on %zu pixels", r.compScale,
            scales.size());
        photoNote += nb2;
      }
      else
      {
        r.compInverse.clear();
        r.compBlack.clear();
        r.compValid.clear();
      }
    }
  }

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
  r.status += photoNote;
  r.pixels = dec;
  return r;
}

std::vector<StructuredLightCalibration::Extra> StructuredLightCalibration::makeExtras(
    int responseLevels, bool compensation, const std::vector<float>& exposures, int index,
    int mixLevels)
{
  std::vector<Extra> out;
  const int K = std::max(0, responseLevels);
  for(int c = 0; c < 4 && K > 0; c++)
    for(int l = 1; l <= K; l++)
    {
      Extra e;
      const float v = float(l) / float(K);
      e.rgb = c < 3 ? std::array<float, 3>{} : std::array<float, 3>{v, v, v};
      if(c < 3)
        e.rgb[c] = v;
      e.kind = RampFrame;
      e.channel = c;
      e.full = compensation && c < 3 && l == K;
      out.push_back(std::move(e));
    }
  if(compensation && K > 0 && mixLevels >= 2)
  {
    const int N = mixLevels;
    auto onRamp = [&](const std::array<float, 3>& c) {
      for(auto& e : out)
        if(e.kind == RampFrame && std::abs(e.rgb[0] - c[0]) < 0.5f / 255.f
           && std::abs(e.rgb[1] - c[1]) < 0.5f / 255.f && std::abs(e.rgb[2] - c[2]) < 0.5f / 255.f)
          return true;
      return false;
    };
    for(int b = 0; b < N; b++)
      for(int g = 0; g < N; g++)
        for(int r = 0; r < N; r++)
        {
          const std::array<float, 3> c{
              float(r) / (N - 1), float(g) / (N - 1), float(b) / (N - 1)};
          if((r == 0 && g == 0 && b == 0) || onRamp(c))
            continue;
          Extra e;
          e.rgb = c;
          e.kind = GridFrame;
          e.channel = 3;
          out.push_back(std::move(e));
        }
  }
  if(compensation && K > 0)
    for(auto rgb :
        {std::array<float, 3>{1.f, 0.6f, 0.3f}, std::array<float, 3>{0.3f, 0.6f, 0.9f},
         std::array<float, 3>{0.9f, 0.9f, 0.4f}, std::array<float, 3>{0.4f, 0.8f, 0.2f},
         std::array<float, 3>{0.7f, 0.2f, 0.6f}, std::array<float, 3>{0.6f, 0.6f, 0.6f}})
    {
      Extra e;
      e.rgb = rgb;
      e.kind = MixFrame;
      e.channel = 3;
      out.push_back(std::move(e));
    }
  if(compensation && K == 0)
    for(int c = 0; c < 3; c++)
    {
      Extra e;
      e.rgb[c] = 1.f;
      e.kind = PrimaryFrame;
      e.channel = c;
      e.full = true;
      out.push_back(std::move(e));
    }
  if(index == 0)
    for(float t : exposures)
    {
      if(!(t > 0.f))
        continue;
      for(float v : {1.f, 0.5f, 0.25f, 0.f})
      {
        Extra e;
        e.rgb = {v, v, v};
        e.exposure = t;
        e.kind = CrfFrame;
        e.channel = 3;
        out.push_back(std::move(e));
      }
    }
  return out;
}

std::shared_ptr<score::gfx::photometric::CameraResponse>
StructuredLightCalibration::solveCameraResponse(const Job& job, double lambda)
{
  namespace ph = score::gfx::photometric;
  std::vector<float> times;
  std::vector<const Extra*> frames;
  for(auto& e : job.extras)
    if(e.kind == CrfFrame && !e.rgbCapture.empty())
    {
      frames.push_back(&e);
      if(std::find(times.begin(), times.end(), e.exposure) == times.end())
        times.push_back(e.exposure);
    }
  if(times.size() < 2)
    return {};
  std::sort(times.begin(), times.end());
  std::vector<float> levels;
  for(auto* e : frames)
    if(std::find(levels.begin(), levels.end(), e->rgb[0]) == levels.end())
      levels.push_back(e->rgb[0]);
  const std::size_t n = frames[0]->rgbCapture.size() / 3;
  const int P = int(times.size());
  constexpr int L = 256;
  auto res = std::make_shared<ph::CameraResponse>();
  res->levels = L;
  std::vector<double> lnDt;
  for(float t : times)
    lnDt.push_back(std::log(double(t)));
  for(int c = 0; c < 3; c++)
  {
    struct Cand
    {
      std::vector<int> z;
      int mid;
    };
    std::vector<Cand> cands;
    for(float lv : levels)
    {
      std::vector<const Extra*> byT(P, nullptr);
      for(auto* e : frames)
        if(e->rgb[0] == lv)
          byT[std::find(times.begin(), times.end(), e->exposure) - times.begin()] = e;
      if(std::find(byT.begin(), byT.end(), nullptr) != byT.end())
        continue;
      for(std::size_t i = 0; i < n; i += 7)
      {
        Cand k;
        k.z.resize(P);
        int good = 0;
        for(int j = 0; j < P; j++)
        {
          const float v = byT[j]->rgbCapture[i * 3 + c];
          k.z[j] = std::isfinite(v) ? int(std::lround(std::clamp(v, 0.f, 1.f) * (L - 1))) : -1;
          good += k.z[j] > 0 && k.z[j] < L - 1;
        }
        if(good < 2)
          continue;
        k.mid = k.z[P / 2];
        cands.push_back(std::move(k));
      }
    }
    if(cands.size() < 16)
      return {};
    std::sort(cands.begin(), cands.end(), [](auto& a, auto& b) { return a.mid < b.mid; });
    const int N = std::min<int>(int(cands.size()), std::max(300, 2 * L / std::max(1, P - 1)));
    std::vector<int> Z;
    for(int k = 0; k < N; k++)
    {
      const auto& cd = cands[std::size_t(k) * (cands.size() - 1) / std::max(1, N - 1)];
      Z.insert(Z.end(), cd.z.begin(), cd.z.end());
    }
    res->g[c] = ph::solveCameraResponse(Z, N, lnDt, L, lambda);
  }
  return res;
}

bool StructuredLightCalibration::saveCameraResponse(
    const std::string& path, const score::gfx::photometric::CameraResponse& crf,
    const std::vector<float>& exposures)
{
  if(!crf.valid())
    return false;
  QJsonObject o;
  o["type"] = "cameraResponse";
  o["version"] = 1;
  o["note"] = "Debevec & Malik 1997: g[c][Z] = ln E + ln dt for code Z = round(v * (levels "
              "- 1)); linear(v) = exp(g(Z) - g(levels - 1))";
  o["levels"] = crf.levels;
  QJsonArray g;
  for(int c = 0; c < 3; c++)
  {
    QJsonArray a;
    for(double v : crf.g[c])
      a.append(v);
    g.append(a);
  }
  o["g"] = g;
  QJsonArray t;
  for(float e : exposures)
    t.append(e);
  o["exposures"] = t;
  QFile f{QString::fromStdString(path)};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(QJsonDocument(o).toJson());
  return true;
}

std::shared_ptr<score::gfx::photometric::CameraResponse>
StructuredLightCalibration::loadCameraResponse(const std::string& path)
{
  QFile f{QString::fromStdString(path)};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  auto r = std::make_shared<score::gfx::photometric::CameraResponse>();
  r->levels = o["levels"].toInt();
  const auto g = o["g"].toArray();
  if(g.size() != 3)
    return {};
  for(int c = 0; c < 3; c++)
    for(const auto& v : g[c].toArray())
      r->g[c].push_back(v.toDouble());
  return r->valid() ? r : nullptr;
}

score::gfx::CompensationMaps StructuredLightCalibration::compensationMaps(const Result& r)
{
  score::gfx::CompensationMaps m;
  const std::size_t n = std::size_t(r.width) * r.height;
  if(r.compInverse.size() != n * 9 || !(r.compScale > 0.f))
    return m;
  m.width = r.width;
  m.height = r.height;
  for(auto& row : m.rows)
  {
    row.width = r.width;
    row.height = r.height;
    row.channels = 4;
    row.data.assign(n * 4, 0.f);
  }
  std::array<float, 3> top{0.f, 0.f, 0.f};
  if(r.compBlackOn)
    for(std::size_t i = 0; i < n; i++)
      if(r.compValid[i])
        for(int c = 0; c < 3; c++)
          top[c] = std::max(top[c], r.compBlack[i * 3 + c]);
  const float k = std::clamp(r.compStrength, 0.f, 1.f);
  const score::gfx::photometric::ColourMixing noMixing;
  const auto& mixing = r.response ? r.response->mixing : noMixing;
  const auto mixInv = score::gfx::photometric::MixingInverse::build(mixing);
  std::vector<float> scale(n, r.compScale);
  if(r.compGradient > 0.f)
  {
    constexpr int B = 16;
    const int w = r.width, h = r.height;
    const int bw = (w + B - 1) / B, bh = (h + B - 1) / B;
    const float inf = std::numeric_limits<float>::infinity();
    std::vector<float> L(std::size_t(bw) * bh, inf);
    std::vector<float> cell;
    for(int by = 0; by < bh; by++)
      for(int bx = 0; bx < bw; bx++)
      {
        cell.clear();
        for(int y = by * B; y < std::min(h, (by + 1) * B); y++)
          for(int x = bx * B; x < std::min(w, (bx + 1) * B); x++)
          {
            const std::size_t i = std::size_t(y) * w + x;
            if(!r.compValid[i])
              continue;
            Eigen::Matrix3d M;
            for(int a = 0; a < 3; a++)
              for(int b = 0; b < 3; b++)
                M(a, b) = r.compInverse[i * 9 + a * 3 + b];
            cell.push_back(float(score::gfx::photometric::maxWhiteScale(M, mixing, mixInv)));
          }
        if(cell.size() < B * B / 4)
          continue;
        const std::size_t q = cell.size() / 2;
        std::nth_element(cell.begin(), cell.begin() + q, cell.end());
        L[std::size_t(by) * bw + bx] = std::log(std::max(cell[q], r.compScale));
      }
    const float g = r.compGradient * B, gd = g * std::sqrt(2.f);
    for(int it = 0; it < 2; it++)
    {
      for(int y = 0; y < bh; y++)
        for(int x = 0; x < bw; x++)
        {
          float& v = L[std::size_t(y) * bw + x];
          if(x > 0)
            v = std::min(v, L[std::size_t(y) * bw + x - 1] + g);
          if(y > 0)
          {
            const std::size_t up = std::size_t(y - 1) * bw + x;
            v = std::min(v, L[up] + g);
            if(x > 0)
              v = std::min(v, L[up - 1] + gd);
            if(x + 1 < bw)
              v = std::min(v, L[up + 1] + gd);
          }
        }
      for(int y = bh - 1; y >= 0; y--)
        for(int x = bw - 1; x >= 0; x--)
        {
          float& v = L[std::size_t(y) * bw + x];
          if(x + 1 < bw)
            v = std::min(v, L[std::size_t(y) * bw + x + 1] + g);
          if(y + 1 < bh)
          {
            const std::size_t dn = std::size_t(y + 1) * bw + x;
            v = std::min(v, L[dn] + g);
            if(x > 0)
              v = std::min(v, L[dn - 1] + gd);
            if(x + 1 < bw)
              v = std::min(v, L[dn + 1] + gd);
          }
        }
    }
    for(auto& v : L)
      if(!std::isfinite(v))
        v = std::log(r.compScale);
    for(int y = 0; y < h; y++)
      for(int x = 0; x < w; x++)
      {
        const float fx = std::clamp((x + 0.5f) / B - 0.5f, 0.f, float(bw - 1));
        const float fy = std::clamp((y + 0.5f) / B - 0.5f, 0.f, float(bh - 1));
        const int x0 = int(fx), y0 = int(fy);
        const int x1 = std::min(x0 + 1, bw - 1), y1 = std::min(y0 + 1, bh - 1);
        const float tx = fx - x0, ty = fy - y0;
        const float a = L[std::size_t(y0) * bw + x0] * (1 - tx) + L[std::size_t(y0) * bw + x1] * tx;
        const float b = L[std::size_t(y1) * bw + x0] * (1 - tx) + L[std::size_t(y1) * bw + x1] * tx;
        scale[std::size_t(y) * w + x] = std::exp(a * (1 - ty) + b * ty);
      }
  }
  for(std::size_t i = 0; i < n; i++)
  {
    const float* Mi = r.compInverse.data() + i * 9;
    const float s = scale[i];
    for(int a = 0; a < 3; a++)
    {
      float* row = m.rows[a].data.data() + i * 4;
      for(int b = 0; b < 3; b++)
        row[b] = r.compValid[i] ? k * s * Mi[a * 3 + b] + (a == b ? 1.f - k : 0.f)
                                : (a == b ? k * s + 1.f - k : 0.f);
      if(r.compBlackOn && r.compValid[i])
      {
        float o = 0.f;
        for(int b = 0; b < 3; b++)
          o += Mi[a * 3 + b] * (top[b] - r.compBlack[i * 3 + b]);
        row[3] = k * o;
      }
    }
  }
  return m;
}

std::vector<StructuredLightCalibration::Result>
StructuredLightCalibration::computeAll(std::vector<Job> jobs)
{
  std::vector<Result> out;
  if(jobs.empty())
    return out;
  std::shared_ptr<const score::gfx::photometric::CameraResponse> crf
      = solveCameraResponse(jobs[0]);
  std::vector<float> exposures;
  for(auto& e : jobs[0].extras)
    if(e.kind == CrfFrame
       && std::find(exposures.begin(), exposures.end(), e.exposure) == exposures.end())
      exposures.push_back(e.exposure);
  for(auto& j : jobs)
  {
    if(crf)
      j.crf = crf;
    for(auto& e : j.extras)
      if(e.kind == CrfFrame)
        e.rgbCapture.clear();
    out.push_back(compute(std::move(j)));
    if(crf)
      out.back().crfExposures = exposures;
  }
  float common = 0.f;
  for(auto& r : out)
    if(r.compScale > 0.f)
      common = common > 0.f ? std::min(common, r.compScale) : r.compScale;
  for(auto& r : out)
    if(r.compScale > 0.f)
      r.compScale = common;
  return out;
}

std::vector<std::array<float, 4>>
StructuredLightCalibration::canvasRects(const std::string& layout, int count)
{
  std::vector<std::array<float, 4>> out;
  const QStringList parts
      = QString::fromStdString(layout).split(';', Qt::SkipEmptyParts);
  for(const auto& p : parts)
  {
    const auto v = p.simplified().split(' ', Qt::SkipEmptyParts);
    if(v.size() != 4)
      continue;
    out.push_back({v[0].toFloat(), v[1].toFloat(), v[2].toFloat(), v[3].toFloat()});
  }
  count = std::max(1, count);
  for(int i = int(out.size()); i < count; i++)
    out.push_back({float(i) / count, 0.f, 1.f / count, 1.f});
  out.resize(count);
  return out;
}

bool StructuredLightCalibration::saveFile(const std::string& path, const Result& r)
{
  if(!r.ok || r.xyz.empty())
    return false;
  const std::string base = SaveFloatTexture::basePath(path);
  if(base.empty())
    return false;
  score::gfx::FloatImage img;
  img.width = r.width;
  img.height = r.height;
  img.channels = 4;
  img.data = r.xyz;
  const QString qbase = QString::fromStdString(base);
  if(!score::gfx::writeFloatImageSplit(qbase, img))
    return false;

  QJsonObject o;
  o["mode"] = QString::fromStdString(slModeName(r.mode));
  o["width"] = r.width;
  o["height"] = r.height;
  QJsonArray k, pose;
  for(float v : r.intrinsics)
    k.append(v);
  for(float v : r.pose)
    pose.append(v);
  o["intrinsics"] = k;
  o["worldFromProjector"] = pose;
  o["rms"] = r.rms;
  o["coverage"] = r.coverage;
  o["intrinsicsMode"] = r.intrinsicsMode == LockIntrinsics      ? "lock"
                        : r.intrinsicsMode == LockToLoaded       ? "lockToLoaded"
                                                                 : "solve";
  o["intrinsicsLocked"] = r.intrinsicsLocked;
  o["viewer"] = QJsonArray{0., 0., 0.};
  QJsonArray planes;
  for(const auto& pl : r.planes)
    planes.append(QJsonArray{pl[0], pl[1], pl[2], pl[3]});
  o["planes"] = planes;
  QJsonObject seq;
  seq["patterns"] = r.seq.count();
  seq["grayBitsX"] = r.seq.projectedX();
  seq["grayBitsY"] = r.seq.projectedY();
  seq["grayBitsSkipped"] = r.seq.skip;
  seq["phaseSteps"] = r.seq.phaseSteps;
  seq["phasePeriod"] = r.seq.period;
  seq["projectorGamma"] = r.seq.gamma;
  seq["phaseValidPercent"] = r.phaseValid;
  o["sequence"] = seq;
  QJsonArray files{
      QFileInfo(qbase + ".xyz.pfm").fileName(), QFileInfo(qbase + ".valid.pfm").fileName()};
  auto writeRGB = [&](const QString& file, const std::vector<float>& rgba) {
    if(rgba.size() != r.xyz.size())
      return false;
    score::gfx::FloatImage m;
    m.width = r.width;
    m.height = r.height;
    m.channels = 3;
    m.data.resize(std::size_t(r.width) * r.height * 3);
    for(std::size_t i = 0; i < m.data.size() / 3; i++)
      for(int c = 0; c < 3; c++)
        m.data[i * 3 + c] = rgba[i * 4 + c];
    if(!score::gfx::writePFM(file, m))
      return false;
    files.append(QFileInfo(file).fileName());
    return true;
  };
  if(writeRGB(qbase + ".lum.pfm", r.lum) && writeRGB(qbase + ".black.pfm", r.black))
  {
    QJsonObject ph;
    ph["luminance"] = QFileInfo(qbase + ".lum.pfm").fileName();
    ph["black"] = QFileInfo(qbase + ".black.pfm").fileName();
    ph["cameraGamma"] = r.cameraGamma;
    ph["note"] = "linear camera RGB per projector pixel: white - black, and black (ambient "
                 "included)";
    if(r.crf)
      ph["cameraResponse"] = QFileInfo(qbase + ".crf.json").fileName();
    o["photometric"] = ph;
  }
  if(r.crf && saveCameraResponse(QString(qbase + ".crf.json").toStdString(), *r.crf, r.crfExposures))
    files.append(QFileInfo(qbase + ".crf.json").fileName());
  if(r.response && score::gfx::writeProjectorResponse(qbase, *r.response))
  {
    files.append(QFileInfo(qbase + ".response.json").fileName());
    files.append(QFileInfo(qbase + ".response.cube").fileName());
  }
  if(const auto comp = compensationMaps(r); comp.valid())
  {
    const score::gfx::photometric::ColourMixing noMixing;
    const auto& mixing = r.response ? r.response->mixing : noMixing;
    const auto mixInv = score::gfx::photometric::MixingInverse::build(mixing);
    score::gfx::CompensationInfo info;
    info.target = {r.compScale, r.compScale, r.compScale};
    info.scale = r.compScale;
    info.mixing = mixing.valid();
    info.strength = r.compStrength;
    info.percentile = r.compGamut;
    info.black = r.compBlackOn;
    std::size_t valid = 0, in = 0;
    for(std::size_t i = 0; i < r.compValid.size(); i++)
    {
      if(!r.compValid[i])
        continue;
      valid++;
      Eigen::Matrix3d M;
      for(int a = 0; a < 3; a++)
        for(int b = 0; b < 3; b++)
          M(a, b) = r.compInverse[i * 9 + a * 3 + b];
      in += score::gfx::photometric::maxWhiteScale(M, mixing, mixInv) >= r.compScale;
    }
    info.inGamut = valid ? double(in) / double(valid) : 0.;
    if(score::gfx::writeCompensation(qbase, comp, info))
      files.append(QFileInfo(qbase + ".comp.json").fileName());
  }
  o["projectorIndex"] = r.projectorIndex;
  o["files"] = files;
  QFile f{qbase + ".json"};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(QJsonDocument(o).toJson());
  return true;
}

StructuredLightCalibration::Result StructuredLightCalibration::loadFile(const std::string& path)
{
  Result r;
  const QString qbase = QString::fromStdString(SaveFloatTexture::basePath(path));
  const auto img = score::gfx::loadFloatImageRGBA(qbase + ".xyz.pfm");
  if(!img.valid())
  {
    r.status = "Failed: cannot read " + QString(qbase + ".xyz.pfm").toStdString();
    return r;
  }
  r.width = img.width;
  r.height = img.height;
  r.xyz = img.data;
  QFile f{qbase + ".json"};
  if(f.open(QIODevice::ReadOnly))
  {
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    r.mode = o["mode"].toString() == "viewer" ? ViewerCamera : DepthCamera;
    const auto k = o["intrinsics"].toArray();
    for(int i = 0; i < 4 && i < k.size(); i++)
      r.intrinsics[i] = float(k[i].toDouble());
    const auto p = o["worldFromProjector"].toArray();
    if(p.size() == 16)
      for(int i = 0; i < 16; i++)
        r.pose[i] = float(p[i].toDouble());
    r.rms = float(o["rms"].toDouble());
    const auto km = o["intrinsicsMode"].toString();
    r.intrinsicsMode = km == "lock" ? LockIntrinsics : km == "lockToLoaded" ? LockToLoaded : SolveIntrinsics;
    r.intrinsicsLocked = o["intrinsicsLocked"].toBool();
    for(const auto& pl : o["planes"].toArray())
    {
      const auto a = pl.toArray();
      if(a.size() == 4)
        r.planes.push_back(
            {float(a[0].toDouble()), float(a[1].toDouble()), float(a[2].toDouble()),
             float(a[3].toDouble())});
    }
  }
  auto readRGBA = [&](const QString& file) {
    std::vector<float> out;
    if(!QFile::exists(file))
      return out;
    const auto m = score::gfx::readPFM(file);
    if(!m.valid() || m.width != r.width || m.height != r.height)
      return out;
    out.resize(std::size_t(r.width) * r.height * 4);
    for(std::size_t i = 0; i < out.size() / 4; i++)
    {
      for(int c = 0; c < 3; c++)
        out[i * 4 + c] = m.data[i * m.channels + (m.channels == 1 ? 0 : c)];
      out[i * 4 + 3] = r.xyz[i * 4 + 3];
    }
    return out;
  };
  r.lum = readRGBA(qbase + ".lum.pfm");
  r.black = readRGBA(qbase + ".black.pfm");
  if(QFile::exists(qbase + ".crf.json"))
    r.crf = loadCameraResponse(QString(qbase + ".crf.json").toStdString());
  std::size_t covered = 0;
  for(std::size_t i = 3; i < r.xyz.size(); i += 4)
    covered += r.xyz[i] > 0.f;
  r.coverage = 100.f * float(covered) / float(std::max<std::size_t>(1, r.xyz.size() / 4));
  r.ok = true;
  r.fromFile = true;
  r.status = "Loaded " + QString(qbase + ".xyz.pfm").toStdString();
  return r;
}

std::function<void(StructuredLightCalibration&)>
StructuredLightCalibration::ins::load_t::process(file_type data)
{
  auto res = std::make_shared<Result>(loadFile(std::string(data.filename)));
  return [res = std::move(res)](StructuredLightCalibration& self) mutable {
    self.m_loaded = std::move(res);
  };
}

struct StructuredLightCalibration::Shared
{
  std::mutex mutex;
  const StructuredLightCalibration* driver{};
  int shown{-1};
  sl::GraySequence seq;
  float level{1.f};
  std::shared_ptr<Result> result;
  std::vector<std::shared_ptr<Result>> results;
  int projector{};
  int projectors{1};
  int resultVersion{};
  std::string status;
  float progress{};
  std::vector<std::array<float, 3>> extraColors;
  float exposure{};
  int exposureVersion{};
  bool autoExposure{};
  int autoVersion{};
};

namespace
{
std::mutex& slRegistryMutex()
{
  static std::mutex m;
  return m;
}
std::map<int64_t, std::weak_ptr<StructuredLightCalibration::Shared>>& slRegistry()
{
  static std::map<int64_t, std::weak_ptr<StructuredLightCalibration::Shared>> r;
  return r;
}
}

void StructuredLightCalibration::prepare(setup s)
{
  if(s.instance == 0)
    return;
  std::lock_guard l{slRegistryMutex()};
  auto& w = slRegistry()[s.instance];
  m_shared = w.lock();
  if(!m_shared)
  {
    m_shared = std::make_shared<Shared>();
    w = m_shared;
  }
}

StructuredLightCalibration::~StructuredLightCalibration()
{
  if(m_future.valid())
    m_future.wait();
  if(m_shared)
  {
    std::lock_guard l{m_shared->mutex};
    if(m_shared->driver == this)
      m_shared->driver = nullptr;
  }
}

void StructuredLightCalibration::mirror()
{
  if(m_phase != Phase::Idle)
    stop(outputs.status.value);
  m_projectors = m_shared->projectors;
  if(m_shared->shown >= 0)
  {
    if(m_projector != m_shared->projector)
    {
      m_projector = m_shared->projector;
      m_shown = -2;
      blankOthers(m_projector);
    }
    m_seq = m_shared->seq;
    m_extraColors = m_shared->extraColors;
    const float level = inputs.level.value;
    inputs.level.value = m_shared->level;
    showPattern(m_shared->shown);
    inputs.level.value = level;
  }
  else
  {
    showIdle();
  }
  if(m_shared->result && m_seenResult != m_shared->resultVersion)
  {
    m_seenResult = m_shared->resultVersion;
    m_results = m_shared->results;
    for(std::size_t i = m_results.size(); i-- > 0;)
      publish(*m_results[i]);
  }
  outputs.status.value = m_shared->status;
  outputs.progress.value = m_shared->progress;
  if(m_seenExposure != m_shared->exposureVersion)
  {
    m_seenExposure = m_shared->exposureVersion;
    outputs.camera_exposure.value = m_shared->exposure;
  }
  if(m_seenAuto != m_shared->autoVersion)
  {
    m_seenAuto = m_shared->autoVersion;
    outputs.auto_exposure.value = m_shared->autoExposure;
  }
}

void StructuredLightCalibration::operator()()
{
  using namespace std::chrono;
  tick(duration<double, std::milli>(steady_clock::now().time_since_epoch()).count());
}

void StructuredLightCalibration::showPattern(int k)
{
  const auto& seq = m_seq;
  if(m_shown == k && m_shownLevel == inputs.level.value && m_shownW == seq.width
     && m_shownH == seq.height)
    return;
  withPatternPort(m_projector, [&](auto& port) {
    port.create(seq.width, seq.height);
    if(k >= seq.count())
    {
      const std::size_t e = std::size_t(k - seq.count());
      const auto c = e < m_extraColors.size() ? m_extraColors[e] : std::array<float, 3>{};
      uint8_t v[3];
      for(int i = 0; i < 3; i++)
        v[i] = uint8_t(std::lround(std::clamp(c[i], 0.f, 1.f) * 255.f));
      auto* px = port.texture.bytes;
      for(std::size_t i = 0; i < std::size_t(seq.width) * seq.height; i++)
      {
        px[i * 4 + 0] = v[0];
        px[i * 4 + 1] = v[1];
        px[i * 4 + 2] = v[2];
        px[i * 4 + 3] = 255;
      }
    }
    else
      sl::renderPattern(seq, k, inputs.level.value, port.texture.bytes);
    port.upload();
  });
  m_canvasDirty = true;
  outputs.projector_index.value = m_projector;
  m_shown = k;
  m_shownIdle = -1;
  m_shownLevel = inputs.level.value;
  m_shownW = seq.width;
  m_shownH = seq.height;
}

void StructuredLightCalibration::showIdle()
{
  const int w = std::max(1, inputs.projector_size.value.x);
  const int h = std::max(1, inputs.projector_size.value.y);
  const int idle = inputs.idle.value;
  if(m_shown == -1 && m_shownIdle == idle && m_shownLevel == inputs.level.value
     && m_shownW == w && m_shownH == h)
    return;
  outputs.pattern.create(w, h);
  auto* px = outputs.pattern.texture.bytes;
  if(idle == TestGrid)
    sl::renderTestGrid(w, h, inputs.level.value, px);
  else
  {
    const uint8_t c = idle == White
                          ? uint8_t(std::lround(std::clamp(inputs.level.value, 0.f, 1.f) * 255.f))
                          : 0;
    for(std::size_t i = 0; i < std::size_t(w) * h; i++)
    {
      px[i * 4 + 0] = px[i * 4 + 1] = px[i * 4 + 2] = c;
      px[i * 4 + 3] = 255;
    }
  }
  outputs.pattern.upload();
  const int count = std::clamp(inputs.projector_count.value, 1, 4);
  for(int p = 1; p < 4; p++)
    withPatternPort(p, [&](auto& port) {
      port.create(w, h);
      if(p < count)
        std::memcpy(port.texture.bytes, px, std::size_t(w) * h * 4);
      else
        std::memset(port.texture.bytes, 0, std::size_t(w) * h * 4);
      port.upload();
    });
  m_canvasDirty = true;
  m_shown = -1;
  m_shownIdle = idle;
  m_shownLevel = inputs.level.value;
  m_shownW = w;
  m_shownH = h;
}

std::string
StructuredLightCalibration::projectorPath(const std::string& path, int index, int count)
{
  if(count <= 1 || index <= 0)
    return path;
  return SaveFloatTexture::basePath(path) + "_p" + std::to_string(index);
}

void StructuredLightCalibration::finishProjector(double now)
{
  m_pendingJobs.push_back(std::move(m_job));
  m_job = Job{};
  if(m_projector + 1 < m_projectors)
  {
    m_projector++;
    m_job.seq = m_seq;
    m_extras = makeExtras(
        inputs.response_levels.value, inputs.compensation.value, {}, m_projector,
        inputs.mix_levels.value);
    m_extraColors.clear();
    for(auto& e : m_extras)
      m_extraColors.push_back(e.rgb);
    m_camW = m_camH = 0;
    m_k = 0;
    m_framesTaken = 0;
    m_frameSum.clear();
    m_rgbSum.clear();
    blankOthers(m_projector);
    m_shown = -2;
    showPattern(0);
    m_shownAt = now;
    m_phase = Phase::Settling;
    inputs.camera.request_readback = true;
    inputs.positions.request_readback = inputs.mode.value == DepthCamera;
    outputs.status.value = "Capturing pattern 1 / " + std::to_string(frameCount())
                           + " of projector " + std::to_string(m_projector + 1) + " / "
                           + std::to_string(m_projectors);
    return;
  }
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  if(inputs.capture_exposure.value > 0.f && m_exposureSent != inputs.capture_exposure.value)
  {
    outputs.camera_exposure.value = inputs.capture_exposure.value;
    m_exposureSent = inputs.capture_exposure.value;
  }
  m_future = std::async(std::launch::async, [jobs = std::move(m_pendingJobs)]() mutable {
    return computeAll(std::move(jobs));
  });
  m_pendingJobs.clear();
  m_phase = Phase::Computing;
  outputs.status.value = "Decoding";
  m_projector = 0;
  showIdle();
}

void StructuredLightCalibration::blankOthers(int projector)
{
  const int w = std::max(1, m_seq.width), h = std::max(1, m_seq.height);
  for(int p = 0; p < 4; p++)
  {
    if(p == projector)
      continue;
    withPatternPort(p, [&](auto& port) {
      port.create(w, h);
      auto* px = port.texture.bytes;
      for(std::size_t i = 0; i < std::size_t(w) * h; i++)
      {
        px[i * 4 + 0] = px[i * 4 + 1] = px[i * 4 + 2] = 0;
        px[i * 4 + 3] = 255;
      }
      port.upload();
    });
  }
  m_canvasDirty = true;
}

void StructuredLightCalibration::updateCanvas()
{
  const int W = inputs.canvas_size.value.x, H = inputs.canvas_size.value.y;
  if(W <= 0 || H <= 0)
    return;
  if(!m_canvasDirty && m_canvasW == W && m_canvasH == H
     && m_canvasLayout == inputs.canvas_layout.value)
    return;
  const int count = std::clamp(inputs.projector_count.value, 1, 4);
  const auto rects = canvasRects(inputs.canvas_layout.value, count);
  outputs.canvas.create(W, H);
  uint8_t* dst = outputs.canvas.texture.bytes;
  for(std::size_t i = 0; i < std::size_t(W) * H; i++)
  {
    dst[i * 4 + 0] = dst[i * 4 + 1] = dst[i * 4 + 2] = 0;
    dst[i * 4 + 3] = 255;
  }
  for(int p = 0; p < count; p++)
    withPatternPort(p, [&](auto& port) {
      const auto& t = port.texture;
      if(!t.bytes || t.width <= 0 || t.height <= 0)
        return;
      const auto& r = rects[p];
      const int x0 = std::clamp(int(std::lround(r[0] * W)), 0, W);
      const int y0 = std::clamp(int(std::lround(r[1] * H)), 0, H);
      const int x1 = std::clamp(int(std::lround((r[0] + r[2]) * W)), 0, W);
      const int y1 = std::clamp(int(std::lround((r[1] + r[3]) * H)), 0, H);
      const double fx = double(t.width) / std::max(1., std::lround(r[2] * W) * 1.);
      const double fy = double(t.height) / std::max(1., std::lround(r[3] * H) * 1.);
      const int ox = int(std::lround(r[0] * W)), oy = int(std::lround(r[1] * H));
      for(int y = y0; y < y1; y++)
      {
        const int sy = std::clamp(int((y - oy + 0.5) * fy), 0, t.height - 1);
        for(int x = x0; x < x1; x++)
        {
          const int sx = std::clamp(int((x - ox + 0.5) * fx), 0, t.width - 1);
          std::memcpy(
              dst + (std::size_t(y) * W + x) * 4,
              t.bytes + (std::size_t(sy) * t.width + sx) * 4, 4);
        }
      }
    });
  outputs.canvas.upload();
  m_canvasDirty = false;
  m_canvasW = W;
  m_canvasH = H;
  m_canvasLayout = inputs.canvas_layout.value;
}

void StructuredLightCalibration::resolveIntrinsics(Job& job) const
{
  job.intrinsicsMode = inputs.intrinsics_mode.value;
  job.lockK = false;
  job.kNote.clear();
  if(job.intrinsicsMode == SolveIntrinsics)
    return;

  std::array<float, 4> k{};
  if(job.intrinsicsMode == LockIntrinsics)
  {
    const auto& v = inputs.locked_intrinsics.value;
    k = {v.x, v.y, v.z, v.w};
    if(!(k[0] > 0.f && k[1] > 0.f) && inputs.throw_ratio.value > 0.f)
    {
      const float W = float(m_seq.width), H = float(m_seq.height);
      const float f = inputs.throw_ratio.value * W;
      k = {f, f, (W - 1.f) / 2.f - inputs.lens_shift.value.x * W,
           (H - 1.f) / 2.f + inputs.lens_shift.value.y * H};
    }
  }
  else
  {
    if(m_loadedK[0] > 0.f)
      k = m_loadedK;
    else if(m_lastSolvedK[0] > 0.f)
      k = m_lastSolvedK;
  }
  if(k[0] > 0.f && k[1] > 0.f)
  {
    job.lockK = true;
    job.lockedK = k;
  }
  else
  {
    job.kNote = job.intrinsicsMode == LockIntrinsics
                    ? "warning: Lock without Locked intrinsics nor Throw ratio, solved "
                      "instead"
                    : "warning: Lock to loaded without a loaded or solved calibration, "
                      "solved instead";
  }
}

void StructuredLightCalibration::publish(const Result& r)
{
  outputs.status.value = r.status;
  if(!r.ok)
    return;
  if(r.fromFile && r.crf)
    m_loadedCrf = r.crf;
  if(r.response && r.projectorIndex == 0)
    for(int c = 0; c < 3; c++)
      outputs.measured_gamma.value[c] = float(r.response->channels[c].gamma);
  if(r.intrinsics[0] > 0.f && r.mode == DepthCamera)
  {
    if(r.fromFile)
      m_loadedK = r.intrinsics;
    else if(!r.intrinsicsLocked)
    {
      m_lastSolvedK = r.intrinsics;
      if(inputs.accumulate_intrinsics.value)
      {
        m_solvedKs.push_back(r.intrinsics);
        if(m_solvedKs.size() > 8)
          m_solvedKs.erase(m_solvedKs.begin());
        for(int c = 0; c < 4; c++)
        {
          std::vector<float> v;
          for(auto& k : m_solvedKs)
            v.push_back(k[c]);
          std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
          float med = v[v.size() / 2];
          if(v.size() % 2 == 0)
            med = 0.5f * (med + *std::max_element(v.begin(), v.begin() + v.size() / 2));
          outputs.accumulated_intrinsics.value[c] = med;
        }
      }
    }
  }
  auto xyzOut = [&](auto& port) {
    port.create(r.width, r.height);
    std::memcpy(port.texture.bytes, r.xyz.data(), r.xyz.size() * sizeof(float));
    port.upload();
  };
  switch(r.projectorIndex)
  {
    case 1:
      xyzOut(outputs.xyz2);
      break;
    case 2:
      xyzOut(outputs.xyz3);
      break;
    case 3:
      xyzOut(outputs.xyz4);
      break;
    default:
      xyzOut(outputs.xyz);
      break;
  }
  if(r.projectorIndex > 0)
    return;
  m_result = std::make_shared<Result>(r);
  auto rgba = [&](auto& port, const std::vector<float>& v) {
    if(v.size() != std::size_t(r.width) * r.height * 4)
      return;
    port.create(r.width, r.height);
    std::memcpy(port.texture.bytes, v.data(), v.size() * sizeof(float));
    port.upload();
  };
  rgba(outputs.lum, r.lum);
  rgba(outputs.black, r.black);
  if(!r.decoded.empty())
  {
    outputs.decoded.create(r.camWidth, r.camHeight);
    std::memcpy(outputs.decoded.texture.bytes, r.decoded.data(), r.decoded.size());
    outputs.decoded.upload();
  }
  outputs.intrinsics.value = r.intrinsics;
  outputs.pose.value = r.pose;
  for(int c = 0; c < 4; c++)
  {
    auto& col = c == 0   ? outputs.m0.value
                : c == 1 ? outputs.m1.value
                : c == 2 ? outputs.m2.value
                         : outputs.m3.value;
    for(int k = 0; k < 4; k++)
      col[k] = r.pose[c * 4 + k];
  }
  outputs.rms.value = r.rms;
  outputs.coverage.value = r.coverage;
  outputs.planes.value = r.planes;
}

void StructuredLightCalibration::saveAll()
{
  std::vector<std::shared_ptr<Result>> all = m_results;
  if(all.empty())
    all.push_back(m_result);
  bool ok = true;
  std::string saved;
  for(std::size_t i = 0; i < all.size(); i++)
  {
    const auto path = projectorPath(inputs.save_path.value, int(i), int(all.size()));
    if(all[i] && saveFile(path, *all[i]))
      saved += (saved.empty() ? "" : ", ") + SaveFloatTexture::basePath(path) + ".xyz.pfm";
    else
      ok = false;
  }
  outputs.status.value
      = ok ? "Saved " + saved : "Failed: cannot save to " + inputs.save_path.value;
}

void StructuredLightCalibration::stop(const std::string& status)
{
  if(m_phase != Phase::Idle && inputs.capture_exposure.value > 0.f
     && m_exposureSent != inputs.capture_exposure.value)
  {
    outputs.camera_exposure.value = inputs.capture_exposure.value;
    m_exposureSent = inputs.capture_exposure.value;
  }
  m_phase = Phase::Idle;
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  outputs.status.value = status;
}

bool StructuredLightCalibration::captureFrame()
{
  const auto& t = inputs.camera.texture;
  if(!t.bytes || t.width <= 0 || t.height <= 0)
    return false;
  if(m_camW == 0)
  {
    m_camW = t.width;
    m_camH = t.height;
    m_job.decoder.reset(m_camW, m_camH, m_seq);
    m_job.decoder.setContrast(inputs.contrast.value);
    m_job.decoder.setPhaseMinModulation(inputs.phase_min_modulation.value);
    const std::size_t n = std::size_t(m_camW) * m_camH;
    m_job.positions.assign(n * 3, 0.f);
    m_job.positionCount.assign(n, 0);
    m_job.zMin.assign(n, 1e30f);
    m_job.zMax.assign(n, -1e30f);
    m_job.positionFrames = 0;
  }
  if(t.width != m_camW || t.height != m_camH)
  {
    stop("Aborted: the camera size changed during the capture");
    return false;
  }
  const std::size_t n = std::size_t(m_camW) * m_camH;
  if(m_frameSum.size() != n)
    m_frameSum.assign(n, 0.f);
  for(std::size_t i = 0; i < n; i++)
  {
    const float* p = t.bytes + i * 4;
    m_frameSum[i] += (p[0] + p[1] + p[2]) * (1.f / 3.f);
  }
  if(m_k <= 1 || m_k >= m_seq.count())
  {
    if(m_rgbSum.size() != n * 3)
      m_rgbSum.assign(n * 3, 0.f);
    for(std::size_t i = 0; i < n; i++)
      for(int c = 0; c < 3; c++)
        m_rgbSum[i * 3 + c] += t.bytes[i * 4 + c];
  }

  const auto& pt = inputs.positions.texture;
  if(inputs.mode.value == DepthCamera && m_k <= 1 && pt.bytes && pt.width > 0
     && pt.height > 0)
  {
    m_job.positionFrames++;
    for(int y = 0; y < m_camH; y++)
    {
      const int py = std::min(pt.height - 1, int((y + 0.5) * pt.height / m_camH));
      for(int x = 0; x < m_camW; x++)
      {
        const int px = std::min(pt.width - 1, int((x + 0.5) * pt.width / m_camW));
        const float* p = pt.bytes + (std::size_t(py) * pt.width + px) * 4;
        if(!(p[3] > 0.f) || !(p[2] > 0.f) || !std::isfinite(p[0]) || !std::isfinite(p[1])
           || !std::isfinite(p[2]))
          continue;
        const std::size_t i = std::size_t(y) * m_camW + x;
        m_job.positions[i * 3] += p[0];
        m_job.positions[i * 3 + 1] += p[1];
        m_job.positions[i * 3 + 2] += p[2];
        m_job.positionCount[i]++;
        m_job.zMin[i] = std::min(m_job.zMin[i], p[2]);
        m_job.zMax[i] = std::max(m_job.zMax[i], p[2]);
      }
    }
  }
  return true;
}

void StructuredLightCalibration::tick(double now)
{
  outputs.done.value.reset();
  outputs.camera_exposure.value.reset();
  outputs.auto_exposure.value.reset();

  std::unique_lock<std::mutex> sharedLock;
  if(m_shared)
  {
    sharedLock = std::unique_lock{m_shared->mutex};
    if(!m_shared->driver)
      m_shared->driver = this;
    if(m_shared->driver != this)
    {
      mirror();
      updateCanvas();
      return;
    }
  }
  struct Publish
  {
    StructuredLightCalibration& self;
    ~Publish()
    {
      self.updateCanvas();
      if(!self.m_shared)
        return;
      auto& sh = *self.m_shared;
      sh.extraColors = self.m_extraColors;
      if(self.outputs.camera_exposure.value)
      {
        sh.exposure = *self.outputs.camera_exposure.value;
        sh.exposureVersion++;
      }
      if(self.outputs.auto_exposure.value)
      {
        sh.autoExposure = *self.outputs.auto_exposure.value;
        sh.autoVersion++;
      }
      sh.shown = self.m_shown;
      sh.seq = self.m_seq;
      sh.level = self.m_shownLevel;
      sh.status = self.outputs.status.value;
      sh.progress = self.outputs.progress.value;
      sh.projector = self.m_projector;
      sh.projectors = self.m_projectors;
      if(self.m_result && sh.result != self.m_result)
      {
        sh.result = self.m_result;
        sh.results = self.m_results;
        if(sh.results.empty())
          sh.results.push_back(self.m_result);
        sh.resultVersion++;
      }
    }
  } publishShared{*this};

  if(m_loaded)
  {
    m_loaded->projectorIndex = 0;
    publish(*m_loaded);
    m_results = {m_result};
    m_loaded.reset();
  }

  if(inputs.save && m_result)
    saveAll();

  if(inputs.abort && m_phase != Phase::Idle)
  {
    if(m_phase == Phase::Computing)
      m_future.wait();
    stop("Aborted");
  }

  if(inputs.start && m_phase == Phase::Idle)
  {
    m_seq = sl::GraySequence::make(
        inputs.projector_size.value.x, inputs.projector_size.value.y,
        inputs.min_stripe.value);
    if(inputs.phase_shift.value)
      m_seq.enablePhase(
          inputs.phase_steps.value, inputs.phase_period.value,
          inputs.projector_gamma.value);
    m_job = Job{};
    m_job.seq = m_seq;
    m_pendingJobs.clear();
    m_projectors = std::clamp(inputs.projector_count.value, 1, 4);
    std::vector<float> exposures;
    for(const auto& t : QString::fromStdString(inputs.exposures.value)
                            .split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts))
      if(const float v = t.toFloat(); v > 0.f)
        exposures.push_back(v);
    m_extras = makeExtras(
        inputs.response_levels.value, inputs.compensation.value, exposures, 0,
        inputs.mix_levels.value);
    m_extraColors.clear();
    for(auto& e : m_extras)
      m_extraColors.push_back(e.rgb);
    if(inputs.crf_file.value != m_crfFileLoaded)
    {
      m_crfFileLoaded = inputs.crf_file.value;
      m_loadedCrf = m_crfFileLoaded.empty() ? nullptr : loadCameraResponse(m_crfFileLoaded);
    }
    m_settleMs = inputs.settle.value;
    if(inputs.capture_exposure.value > 0.f)
    {
      outputs.camera_exposure.value = inputs.capture_exposure.value;
      m_exposureSent = inputs.capture_exposure.value;
      m_settleMs = std::max(m_settleMs, double(inputs.exposure_settle.value));
    }
    if(inputs.capture_exposure.value > 0.f || !exposures.empty())
      outputs.auto_exposure.value = false;
    m_projector = 0;
    m_rgbSum.clear();
    m_shown = -2;
    blankOthers(0);
    m_camW = m_camH = 0;
    m_k = 0;
    m_framesTaken = 0;
    m_frameSum.clear();
    m_phase = Phase::Settling;
    m_shownAt = now;
    inputs.camera.request_readback = true;
    inputs.positions.request_readback = inputs.mode.value == DepthCamera;
    showPattern(0);
    outputs.status.value = "Capturing pattern 1 / " + std::to_string(frameCount());
    if(m_projectors > 1)
      outputs.status.value += " of projector 1 / " + std::to_string(m_projectors);
    if(m_seq.phaseSteps > 0)
      outputs.status.value += " (phase " + std::to_string(m_seq.phaseSteps)
                              + " steps, period " + std::to_string(m_seq.period) + " px)";
  }

  switch(m_phase)
  {
    case Phase::Idle:
      showIdle();
      outputs.progress.value = m_result ? 1.f : 0.f;
      return;

    case Phase::Settling:
      if(now - m_shownAt >= m_settleMs)
      {
        m_phase = Phase::Capturing;
        m_framesTaken = 0;
        m_frameSum.clear();
      }
      break;

    case Phase::Capturing:
      if(now - m_lastFrameAt >= 30. && captureFrame())
      {
        m_lastFrameAt = now;
        m_framesTaken++;
        if(m_framesTaken >= std::max(1, inputs.frames.value))
        {
          for(float& v : m_frameSum)
            v /= float(m_framesTaken);
          if(static const bool trace = qEnvironmentVariableIsSet("SCORE_SL_TRACE"); trace)
          {
            double mean = 0.;
            uint64_t hash = 1469598103934665603ull;
            for(float v : m_frameSum)
            {
              mean += v;
              uint32_t b;
              std::memcpy(&b, &v, 4);
              hash = (hash ^ b) * 1099511628211ull;
            }
            qDebug("[sl-trace] pattern %d mean %.5f hash %016llx", m_k,
                   mean / std::max<std::size_t>(1, m_frameSum.size()),
                   (unsigned long long)hash);
          }
          if(m_k == 0)
            m_job.whiteMax = *std::max_element(m_frameSum.begin(), m_frameSum.end());
          if(m_k == 0 && m_job.whiteMax <= 1e-6f)
          {
            stop("Failed: the camera image was black during the white pattern: the "
                 "camera delivered no frame (is it streaming?)");
            return;
          }
          const bool extra = m_k >= m_seq.count();
          if((m_k <= 1 || extra) && m_rgbSum.size() == m_frameSum.size() * 3)
          {
            for(float& v : m_rgbSum)
              v /= float(m_framesTaken);
            if(!extra)
              (m_k == 0 ? m_job.whiteRGB : m_job.blackRGB) = std::move(m_rgbSum);
            else if(auto& e = m_extras[m_k - m_seq.count()]; e.full)
              e.rgbCapture = std::move(m_rgbSum);
            else
            {
              const int S = std::max(1, m_job.extraStride);
              const int sw = (m_camW + S - 1) / S, shh = (m_camH + S - 1) / S;
              e.rgbCapture.resize(std::size_t(sw) * shh * 3);
              for(int y = 0; y < shh; y++)
                for(int x = 0; x < sw; x++)
                  for(int c = 0; c < 3; c++)
                    e.rgbCapture[(std::size_t(y) * sw + x) * 3 + c]
                        = m_rgbSum[(std::size_t(y * S) * m_camW + x * S) * 3 + c];
            }
          }
          m_rgbSum.clear();
          if(!extra)
            m_job.decoder.add(m_k, m_frameSum);
          m_k++;
          if(m_k >= frameCount())
          {
            inputs.camera.request_readback = false;
            inputs.positions.request_readback = false;
            m_job.worldFromCamera = inputs.world_from_camera.value;
            auto k = inputs.intrinsics.value;
            m_job.cameraIntrinsics = {k.x, k.y, k.z, k.w};
            if(!(k.x > 0.f) || !(k.y > 0.f))
            {
              const float f = float(
                  m_camW / (2. * std::tan(inputs.fov.value * 0.5 * M_PI / 180.)));
              m_job.cameraIntrinsics
                  = {f, f, (m_camW - 1) * 0.5f, (m_camH - 1) * 0.5f};
            }
            m_job.mode = inputs.mode.value;
            m_job.minSignal = inputs.min_signal.value;
            m_job.minBits = inputs.min_bits.value;
            m_job.holeRadius = inputs.hole_radius.value;
            m_job.depthJump = inputs.depth_jump.value;
            m_job.ransac = inputs.ransac.value;
            resolveIntrinsics(m_job);
            m_job.surface = inputs.surface.value;
            m_job.maxPlanes = inputs.max_planes.value;
            m_job.planeThreshold = inputs.plane_threshold.value;
            m_job.cameraGamma = inputs.camera_gamma.value;
            m_job.projectorIndex = m_projector;
            m_job.extras = std::move(m_extras);
            m_extras.clear();
            m_job.crf = m_loadedCrf;
            m_job.compGamut = inputs.comp_gamut.value;
            m_job.compStrength = inputs.comp_strength.value;
            m_job.compGradient = inputs.comp_gradient.value;
            m_job.mixLevels = inputs.compensation.value && inputs.response_levels.value > 0
                                  ? inputs.mix_levels.value
                                  : 0;
            m_job.compBlack = inputs.comp_black.value;
            finishProjector(now);
          }
          else
          {
            showPattern(m_k);
            m_shownAt = now;
            m_settleMs = inputs.settle.value;
            if(m_k >= m_seq.count())
              if(const auto& e = m_extras[m_k - m_seq.count()];
                 e.exposure > 0.f && e.exposure != m_exposureSent)
              {
                outputs.camera_exposure.value = e.exposure;
                m_exposureSent = e.exposure;
                m_settleMs = std::max(m_settleMs, double(inputs.exposure_settle.value));
              }
            m_phase = Phase::Settling;
            inputs.positions.request_readback
                = inputs.mode.value == DepthCamera && m_k <= 1;
            outputs.status.value = "Capturing pattern " + std::to_string(m_k + 1) + " / "
                                   + std::to_string(frameCount());
            if(m_projectors > 1)
              outputs.status.value += " of projector " + std::to_string(m_projector + 1)
                                      + " / " + std::to_string(m_projectors);
          }
        }
      }
      break;

    case Phase::Computing:
      if(m_future.valid()
         && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      {
        auto all = m_future.get();
        m_results.clear();
        std::string status;
        bool ok = !all.empty();
        for(std::size_t i = all.size(); i-- > 0;)
        {
          publish(all[i]);
          ok &= all[i].ok;
          if(all.size() > 1)
            status = "P" + std::to_string(i + 1) + ": " + all[i].status
                     + (status.empty() ? "" : " | " + status);
          else
            status = all[i].status;
        }
        for(auto& r : all)
          m_results.push_back(std::make_shared<Result>(std::move(r)));
        if(!m_results.empty())
          m_result = m_results[0];
        outputs.status.value = status;
        m_phase = Phase::Idle;
        if(ok && inputs.auto_save.value && !inputs.save_path.value.empty())
        {
          saveAll();
          outputs.status.value = status + " | " + outputs.status.value;
        }
        if(ok)
          outputs.done.value = halp::impulse{};
      }
      break;
  }

  const int n = std::max(1, frameCount());
  const float within = m_phase == Phase::Capturing
                           ? float(m_framesTaken) / std::max(1, inputs.frames.value)
                           : 0.f;
  outputs.progress.value
      = m_phase == Phase::Computing
            ? 0.99f
            : std::min(0.99f, (m_projector * n + m_k + within) / float(n * m_projectors));
}
}
