#include "StructuredLightCore.hpp"

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>

namespace Threedim
{
namespace
{
constexpr float clipLevel = 0.985f;

void throwIfCancelled(const StructuredLightCore::Job& job)
{
  if(job.cancel && job.cancel->load(std::memory_order_relaxed))
    throw std::runtime_error("cancelled");
}
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

std::shared_ptr<const score::gfx::photometric::CameraResponse>
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

bool StructuredLightCore::plausible(const score::gfx::photometric::CameraResponse& crf)
{
  return score::gfx::photometric::plausibleResponse(crf, 0.5 * std::log(7.));
}

QByteArray StructuredLightCore::encodeCameraResponse(
    const score::gfx::photometric::CameraResponse& crf, const std::vector<float>& exposures)
{
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
  return QJsonDocument(o).toJson();
}

std::shared_ptr<const score::gfx::photometric::CameraResponse>
StructuredLightCore::decodeCameraResponse(const QByteArray& json)
{
  const auto o = QJsonDocument::fromJson(json).object();
  auto r = std::make_shared<score::gfx::photometric::CameraResponse>();
  r->levels = o["levels"].toInt();
  const auto g = o["g"].toArray();
  if(g.size() != 3)
    return {};
  for(int c = 0; c < 3; c++)
    for(const auto& v : g[c].toArray())
      r->g[c].push_back(v.toDouble());
  if(!r->valid())
    return {};
  return r;
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
    {
      const auto bound = L;
      for(int y = 0; y < bh; y++)
        for(int x = 0; x < bw; x++)
          for(int dy = -1; dy <= 1; dy++)
            for(int dx = -1; dx <= 1; dx++)
            {
              const int xx = x + dx, yy = y + dy;
              if(xx >= 0 && yy >= 0 && xx < bw && yy < bh)
                L[std::size_t(y) * bw + x] = std::min(
                    L[std::size_t(y) * bw + x], bound[std::size_t(yy) * bw + xx]);
            }
    }
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

void StructuredLightCore::computePhotometric(
    const Job& job, const std::vector<sl::DecodedPixel>& dec, Result& r, std::string& note)
{
  const int cw = job.decoder.width(), ch = job.decoder.height();
  const std::size_t camN = std::size_t(cw) * ch;
  const bool rgb = job.whiteRGB.size() == camN * 3 && job.blackRGB.size() == camN * 3;
  const float camGamma = job.cameraGamma > 0.f ? job.cameraGamma : 1.f;
  auto lin = [&](int c, float v) -> float {
    if(job.crf)
      return float(job.crf->linear(c, v));
    return camGamma == 1.f ? v : std::pow(std::max(0.f, v), camGamma);
  };
  auto usable = [&](std::size_t i) {
    if(i >= dec.size() || !dec[i].valid || dec[i].rim || dec[i].saturated)
      return false;
    if(rgb)
      for(int c = 0; c < 3; c++)
        if(job.whiteRGB[i * 3 + c] >= clipLevel)
          return false;
    return true;
  };
  const int S = std::max(1, job.extraStride);
  const int sw = (cw + S - 1) / S, shh = (ch + S - 1) / S;

  if(rgb)
  {
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
      note += nb2;
      if(pr.additivityError >= 0.)
      {
        std::snprintf(nb2, sizeof(nb2), " (additivity error %.1f%%)", 100. * pr.additivityError);
        note += nb2;
      }
      if(pr.mixing.valid())
      {
        const auto x = pr.mixing.forward({1., 1., 1.});
        std::snprintf(
            nb2, sizeof(nb2), ", white shown at %.2f %.2f %.2f of the primaries' sum", x[0],
            x[1], x[2]);
        note += nb2;
      }
      if(pr.mixErrorAdditive >= 0.)
      {
        std::snprintf(nb2, sizeof(nb2), ", mixed colours error %.1f%% additive", 100. * pr.mixErrorAdditive);
        note += nb2;
        if(pr.mixErrorModel >= 0.)
        {
          std::snprintf(nb2, sizeof(nb2), ", %.1f%% with the measured mixing", 100. * pr.mixErrorModel);
          note += nb2;
        }
      }
      if(pr.channels[0].valid && pr.channels[1].valid && pr.channels[2].valid)
        r.response = std::move(pr);
      else
        note += ", projector response not fitted: a channel gave no light";
    }

    throwIfCancelled(job);
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
        note += nb2;
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

}

void StructuredLightCore::addPhotometricFiles(
    const Result& r, CalibrationWrite& w, QJsonObject& photometric)
{
  const QString name = QFileInfo{w.base}.fileName();
  if(r.crf)
  {
    w.files.push_back({".crf.json", encodeCameraResponse(*r.crf, r.crfExposures)});
    photometric["cameraResponse"] = QString(name + ".crf.json");
  }
  if(r.response)
    for(auto& f : score::gfx::encodeProjectorResponse(*r.response))
      w.files.push_back(std::move(f));
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
    auto files = score::gfx::encodeCompensation(name, comp, info);
    for(auto& [suffix, img] : files.maps)
      w.maps.push_back({suffix, std::move(img)});
    w.files.push_back(std::move(files.json));
  }
}

void StructuredLightCore::loadPhotometricFiles(
    const QString& base, Result& r, std::string& note)
{
  const QString file = calibrationMapFile(base, ".crf.json");
  if(file.isEmpty())
    return;
  QFile f{file};
  if(!f.open(QIODevice::ReadOnly))
    return;
  auto crf = decodeCameraResponse(f.readAll());
  if(crf && plausible(*crf))
    r.crf = std::move(crf);
  else
    note += "; camera response " + file.toStdString() + " rejected";
}
}
