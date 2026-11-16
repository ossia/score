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
#include <QSaveFile>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Threedim
{
namespace procam = score::gfx::procam;

namespace
{
std::string slModeName(int mode)
{
  return mode == StructuredLightCore::ViewerCamera ? "viewer" : "depth";
}

std::array<float, 3> slMul(const Eigen::Matrix4d& m, float x, float y, float z, float w)
{
  const Eigen::Vector4d r = m * Eigen::Vector4d(x, y, z, w);
  return {float(r.x()), float(r.y()), float(r.z())};
}
}

namespace
{
constexpr float clipLevel = 0.985f;

void throwIfCancelled(const StructuredLightCore::Job& job)
{
  if(job.cancel && job.cancel->load(std::memory_order_relaxed))
    throw std::runtime_error("cancelled");
}
}

StructuredLightCore::Result StructuredLightCore::compute(Job job)
{
  Result r;
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
        if(job.whiteRGB[i * 3 + c] >= clipLevel)
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
      if(pr.channels[0].valid && pr.channels[1].valid && pr.channels[2].valid)
        r.response = std::move(pr);
      else
        photoNote += ", projector response not fitted: a channel gave no light";
    }

    throwIfCancelled(job);
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
      auto clipped = [&](std::size_t i) {
        for(int p = 0; p < 3; p++)
          for(int c = 0; c < 3; c++)
            if(prim[p]->rgbCapture[i * 3 + c] >= clipLevel)
              return true;
        return false;
      };
      for(std::size_t i = 0; i < dec.size(); i++)
      {
        if(!usable(i) || clipped(i))
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
      cols = {};
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
      r.compTarget = target;
      r.compWhite.assign(n, 0.f);
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
        r.compWhite[i] = float(score::gfx::photometric::maxWhiteScale(M, mixing, mixInv));
        scales.push_back(r.compWhite[i]);
      }
      filled = {};
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
            Eigen::Matrix3d M;
            for(int a = 0; a < 3; a++)
              for(int b = 0; b < 3; b++)
                M(a, b) = Mi[a * 3 + b] = f[a][i * 4 + b];
            for(int c = 0; c < 3; c++)
              r.compBlack[i * 3 + c] = f[3][i * 4 + c];
            r.compValid[i] = 1;
            r.compWhite[i] = float(score::gfx::photometric::maxWhiteScale(M, mixing, mixInv));
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
        r.compWhite.clear();
      }
    }
  }

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
  r.pixels = std::move(dec);
  return r;
}

std::vector<StructuredLightCore::Extra> StructuredLightCore::makeExtras(
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
StructuredLightCore::solveCameraResponse(const Job& job, double lambda)
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

bool StructuredLightCore::saveCameraResponse(
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
StructuredLightCore::loadCameraResponse(const std::string& path)
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

std::vector<float> StructuredLightCore::compensationScale(const Result& r)
{
  const std::size_t n = std::size_t(r.width) * r.height;
  std::vector<float> scale(n, r.compScale);
  if(r.compGradient > 0.f && r.compWhite.size() == n && r.compValid.size() == n)
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
            if(r.compValid[i])
              cell.push_back(r.compWhite[i]);
          }
        if(cell.size() < B * B / 4)
          continue;
        const std::size_t q = cell.size() / 2;
        std::nth_element(cell.begin(), cell.begin() + q, cell.end());
        L[std::size_t(by) * bw + bx] = std::log(std::max(cell[q], r.compScale));
      }
    const float g = r.compGradient * B, gd = g * std::sqrt(2.f);
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
  return scale;
}

score::gfx::CompensationMaps StructuredLightCore::compensationMaps(const Result& r)
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
  const auto scale = compensationScale(r);
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

std::vector<StructuredLightCore::Result>
StructuredLightCore::computeAll(std::vector<Job> jobs)
{
  std::vector<Result> out;
  if(jobs.empty())
    return out;
  std::shared_ptr<const score::gfx::photometric::CameraResponse> crf
      = solveCameraResponse(jobs[0]);
  bool crfRejected = false;
  if(crf && !score::gfx::photometric::plausibleResponse(*crf, 0.5 * std::log(7.)))
  {
    crf.reset();
    crfRejected = true;
  }
  std::vector<float> exposures;
  for(auto& e : jobs[0].extras)
    if(e.kind == CrfFrame
       && std::find(exposures.begin(), exposures.end(), e.exposure) == exposures.end())
      exposures.push_back(e.exposure);
  for(auto& j : jobs)
  {
    throwIfCancelled(j);
    if(crf)
      j.crf = crf;
    j.crfRejected = crfRejected;
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
    {
      r.compScale = common;
      if(out.size() > 1)
        r.compGradient = 0.f;
    }
  return out;
}

std::vector<std::array<float, 4>>
StructuredLightCore::canvasRects(const std::string& layout, int count)
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

bool StructuredLightCore::saveFile(const std::string& path, const Result& r)
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
  if(r.intrinsics[0] > 0.f)
  {
    QJsonArray k, pose;
    for(float v : r.intrinsics)
      k.append(v);
    for(float v : r.pose)
      pose.append(v);
    o["intrinsics"] = k;
    o["worldFromProjector"] = pose;
  }
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
    score::gfx::CompensationInfo info;
    for(int c = 0; c < 3; c++)
      info.target[c] = r.compScale * r.compTarget[c];
    info.scale = r.compScale;
    info.mixing = r.response && r.response->mixing.valid();
    info.strength = r.compStrength;
    info.percentile = r.compGamut;
    info.black = r.compBlackOn;
    const auto scale = compensationScale(r);
    std::size_t valid = 0, in = 0;
    for(std::size_t i = 0; i < r.compValid.size() && i < r.compWhite.size(); i++)
    {
      if(!r.compValid[i])
        continue;
      valid++;
      in += r.compWhite[i] >= scale[i];
    }
    info.inGamut = valid ? double(in) / double(valid) : 0.;
    if(score::gfx::writeCompensation(qbase, comp, info))
      files.append(QFileInfo(qbase + ".comp.json").fileName());
  }
  o["projectorIndex"] = r.projectorIndex;
  o["files"] = files;
  QSaveFile f{qbase + ".json"};
  if(!f.open(QIODevice::WriteOnly))
    return false;
  f.write(QJsonDocument(o).toJson());
  return f.commit();
}

StructuredLightCore::Result StructuredLightCore::loadFile(const std::string& path)
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

std::string
StructuredLightCore::projectorPath(const std::string& path, int index, int count)
{
  if(count <= 1 || index <= 0)
    return path;
  return SaveFloatTexture::basePath(path) + "_p" + std::to_string(index);
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
  bool running{};
  int shown{-1};
  sl::GraySequence seq;
  float level{1.f};
  std::vector<std::shared_ptr<const Result>> results;
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

std::vector<float> parseExposures(const std::string& text)
{
  std::vector<float> out;
  for(const auto& t : QString::fromStdString(text).split(
          QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts))
    if(const float v = t.toFloat(); v > 0.f)
      out.push_back(v);
  return out;
}
}

void StructuredLightCalibration::prepare(setup s)
{
  if(s.instance == 0)
    return;
  std::lock_guard l{slRegistryMutex()};
  std::erase_if(slRegistry(), [](const auto& kv) { return kv.second.expired(); });
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
  if(m_shared)
  {
    std::lock_guard l{m_shared->mutex};
    if(m_shared->driver == this)
      m_shared->driver = nullptr;
  }
}

StructuredLightSession::Settings StructuredLightCalibration::settings() const
{
  StructuredLightSession::Settings s;
  s.projectorWidth = inputs.projector_size.value.x;
  s.projectorHeight = inputs.projector_size.value.y;
  s.projectors = std::clamp(inputs.projector_count.value, 1, 16);
  s.mode = inputs.mode.value;
  s.fov = inputs.fov.value;
  const auto& k = inputs.intrinsics.value;
  s.cameraIntrinsics = {k.x, k.y, k.z, k.w};
  s.worldFromCamera = inputs.world_from_camera.value;
  s.level = inputs.level.value;
  s.minStripe = inputs.min_stripe.value;
  s.settleMs = inputs.settle.value;
  s.frames = inputs.frames.value;
  s.contrast = inputs.contrast.value;
  s.minSignal = inputs.min_signal.value;
  s.minBits = inputs.min_bits.value;
  s.holeRadius = inputs.hole_radius.value;
  s.depthJump = inputs.depth_jump.value;
  s.ransac = inputs.ransac.value;
  s.idle = inputs.idle.value;
  s.surface = inputs.surface.value;
  s.maxPlanes = inputs.max_planes.value;
  s.planeThreshold = inputs.plane_threshold.value;
  s.phaseShift = inputs.phase_shift.value;
  s.phaseSteps = inputs.phase_steps.value;
  s.phasePeriod = inputs.phase_period.value;
  s.projectorGamma = inputs.projector_gamma.value;
  s.phaseMinModulation = inputs.phase_min_modulation.value;
  s.intrinsicsMode = inputs.intrinsics_mode.value;
  const auto& lk = inputs.locked_intrinsics.value;
  s.lockedIntrinsics = {lk.x, lk.y, lk.z, lk.w};
  s.throwRatio = inputs.throw_ratio.value;
  s.lensShift = {inputs.lens_shift.value.x, inputs.lens_shift.value.y};
  s.accumulateIntrinsics = inputs.accumulate_intrinsics.value;
  s.cameraGamma = inputs.camera_gamma.value;
  s.responseLevels = inputs.response_levels.value;
  s.compensation = inputs.compensation.value;
  s.exposureSettleMs = inputs.exposure_settle.value;
  s.captureExposure = inputs.capture_exposure.value;
  s.crfFile = inputs.crf_file.value;
  s.compGamut = inputs.comp_gamut.value;
  s.compStrength = inputs.comp_strength.value;
  s.compGradient = inputs.comp_gradient.value;
  s.compBlack = inputs.comp_black.value;
  s.mixLevels = inputs.mix_levels.value;
  s.savePath = inputs.save_path.value;
  s.autoSave = inputs.auto_save.value;
  return s;
}

void StructuredLightCalibration::mirror()
{
  if(m_session.running())
  {
    StructuredLightSession::Events ignored;
    m_session.abort(settings(), ignored, outputs.status.value);
  }
  if(m_shared->shown >= 0)
    m_session.showPattern(
        m_shared->seq, m_shared->projector, m_shared->projectors, m_shared->shown,
        m_shared->level, m_shared->extraColors);
  else
    m_session.showIdle(
        inputs.projector_size.value.x, inputs.projector_size.value.y, inputs.idle.value,
        inputs.level.value, std::clamp(inputs.projector_count.value, 1, 16));
  if(!m_shared->results.empty() && m_seenResult != m_shared->resultVersion)
  {
    m_seenResult = m_shared->resultVersion;
    m_session.mirrorResults(m_shared->results);
  }
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

void StructuredLightCalibration::syncPorts()
{
  const auto& d = m_session.display();
  if(d.width <= 0 || d.height <= 0)
    return;
  if(!d.idle)
    outputs.projector_index.value = d.projector;
  for(int p = 0; p < 4; p++)
  {
    const bool image = d.idle ? (p == 0 || p < d.projectors) : p == d.projector;
    const PortState want{image ? 1 : 0, image ? d.version : 0, d.width, d.height};
    auto& have = m_ports[p];
    if(have.kind == want.kind && have.version == want.version && have.width == want.width
       && have.height == want.height)
      continue;
    have = want;
    auto write = [&](auto& port) {
      port.create(d.width, d.height);
      auto* px = port.texture.bytes;
      const std::size_t n = std::size_t(d.width) * d.height;
      if(image)
        std::memcpy(px, d.rgba.data(), n * 4);
      else
        for(std::size_t i = 0; i < n; i++)
        {
          px[i * 4 + 0] = px[i * 4 + 1] = px[i * 4 + 2] = 0;
          px[i * 4 + 3] = 255;
        }
      port.upload();
    };
    switch(p)
    {
      case 1:
        write(outputs.pattern2);
        break;
      case 2:
        write(outputs.pattern3);
        break;
      case 3:
        write(outputs.pattern4);
        break;
      default:
        write(outputs.pattern);
        break;
    }
  }
}

void StructuredLightCalibration::updateCanvas()
{
  const int W = inputs.canvas_size.value.x, H = inputs.canvas_size.value.y;
  if(W <= 0 || H <= 0)
    return;
  const auto& d = m_session.display();
  const int count = std::max(1, d.projectors);
  if(m_canvasVersion == d.version && m_canvasW == W && m_canvasH == H
     && m_canvasCount == count && m_canvasLayout == inputs.canvas_layout.value)
    return;
  outputs.canvas.create(W, H);
  m_session.renderCanvas(W, H, inputs.canvas_layout.value, count, outputs.canvas.texture.bytes);
  outputs.canvas.upload();
  m_canvasVersion = d.version;
  m_canvasW = W;
  m_canvasH = H;
  m_canvasCount = count;
  m_canvasLayout = inputs.canvas_layout.value;
}

void StructuredLightCalibration::writeEvents(const StructuredLightSession::Events& ev)
{
  if(ev.exposure)
    outputs.camera_exposure.value = ev.exposure;
  if(ev.autoExposure)
    outputs.auto_exposure.value = ev.autoExposure;
  if(ev.done)
    outputs.done.value = halp::impulse{};
}

void StructuredLightCalibration::publish(const Result& r)
{
  outputs.status.value = r.status;
  if(!r.ok)
    return;
  outputs.measured_gamma.value = m_session.measuredGamma();
  if(inputs.accumulate_intrinsics.value)
    outputs.accumulated_intrinsics.value = m_session.accumulatedIntrinsics();
  auto xyzOut = [&](auto& port) {
    port.create(r.width, r.height);
    std::memcpy(port.texture.bytes, r.xyz.data(), r.xyz.size() * sizeof(float));
    port.upload();
  };
  switch(r.projectorIndex)
  {
    case 0:
      xyzOut(outputs.xyz);
      break;
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
      break;
  }
  if(r.projectorIndex > 0)
    return;
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
  if(r.intrinsics[0] > 0.f)
  {
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
  }
  outputs.rms.value = r.rms;
  outputs.coverage.value = r.coverage;
  outputs.planes.value = r.planes;
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
    {
      m_shared->driver = this;
      if(m_shared->running)
      {
        m_shared->running = false;
        m_session.setStatus("Aborted: the instance running the calibration was removed");
      }
    }
    if(m_shared->driver != this)
    {
      mirror();
      if(m_published != m_session.resultVersion())
      {
        m_published = m_session.resultVersion();
        const auto& all = m_session.results();
        for(std::size_t i = all.size(); i-- > 0;)
          publish(*all[i]);
      }
      outputs.status.value = m_shared->status;
      syncPorts();
      updateCanvas();
      return;
    }
  }

  const auto s = settings();
  StructuredLightSession::Events ev;
  if(m_loaded)
  {
    m_session.adopt(std::move(m_loaded), s.accumulateIntrinsics);
    m_loaded.reset();
  }
  if(inputs.save && !m_session.results().empty())
    m_session.setStatus(m_session.saveAll(inputs.save_path.value));
  if(inputs.abort && m_session.running())
    m_session.abort(s, ev);
  if(inputs.start && !m_session.running())
  {
    auto first = s;
    first.exposures = parseExposures(inputs.exposures.value);
    m_session.start(first, now, ev);
  }

  StructuredLightSession::Frame frame;
  if(const auto& t = inputs.camera.texture; t.bytes && t.width > 0 && t.height > 0)
  {
    frame.rgba = t.bytes;
    frame.width = t.width;
    frame.height = t.height;
  }
  if(const auto& t = inputs.positions.texture; t.bytes && t.width > 0 && t.height > 0)
  {
    frame.positions = t.bytes;
    frame.positionsWidth = t.width;
    frame.positionsHeight = t.height;
  }
  m_session.tick(s, now, frame, ev);
  inputs.camera.request_readback = m_session.wantsCamera();
  inputs.positions.request_readback = m_session.wantsPositions();
  writeEvents(ev);

  if(m_published != m_session.resultVersion())
  {
    m_published = m_session.resultVersion();
    const auto& all = m_session.results();
    for(std::size_t i = all.size(); i-- > 0;)
      publish(*all[i]);
  }
  outputs.status.value = m_session.status();
  outputs.progress.value = m_session.progress();
  syncPorts();
  updateCanvas();

  if(!m_shared)
    return;
  auto& sh = *m_shared;
  sh.extraColors = m_session.extraColors();
  if(outputs.camera_exposure.value)
  {
    sh.exposure = *outputs.camera_exposure.value;
    sh.exposureVersion++;
  }
  if(outputs.auto_exposure.value)
  {
    sh.autoExposure = *outputs.auto_exposure.value;
    sh.autoVersion++;
  }
  sh.running = m_session.running();
  sh.shown = m_session.shown();
  sh.seq = m_session.sequence();
  sh.level = m_session.shownLevel();
  sh.status = outputs.status.value;
  sh.progress = outputs.progress.value;
  sh.projector = m_session.projector();
  sh.projectors = m_session.projectors();
  if(m_seenResult != m_session.resultVersion())
  {
    m_seenResult = m_session.resultVersion();
    sh.results = m_session.results();
    sh.resultVersion++;
  }
}
}
