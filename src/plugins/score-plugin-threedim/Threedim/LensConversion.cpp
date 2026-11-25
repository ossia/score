#include "LensConversion.hpp"

#include <Threedim/PlaneCompletion.hpp>

#include <Threedim/CalibrationFiles.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace Threedim
{
bool saveMeasuredCamera(const QString& base, const MeasuredCamera& c)
{
  QJsonObject o;
  o["intrinsics"]
      = QJsonArray{c.intrinsics[0], c.intrinsics[1], c.intrinsics[2], c.intrinsics[3]};
  o["width"] = c.width;
  o["height"] = c.height;
  o["lens"] = QString::fromLatin1(CameraLens::name(c.lens.model));
  QJsonArray k;
  for(double v : c.lens.k)
    k.append(v);
  o["k"] = k;
  return writeJsonFile(base + QStringLiteral(".measured.json"), o);
}

std::optional<MeasuredCamera> loadMeasuredCamera(const QString& base)
{
  const auto parsed = parseJsonFile(base + QStringLiteral(".measured.json"));
  if(!parsed)
    return std::nullopt;
  const auto& o = *parsed;
  MeasuredCamera c;
  const auto k = o["intrinsics"].toArray();
  if(k.size() != 4)
    return std::nullopt;
  for(int i = 0; i < 4; i++)
    c.intrinsics[i] = k[i].toDouble();
  c.width = o["width"].toInt();
  c.height = o["height"].toInt();
  c.lens.model = CameraLens::fromName(o["lens"].toString().toStdString());
  const auto lk = o["k"].toArray();
  for(int i = 0; i < 5 && i < lk.size(); i++)
    c.lens.k[i] = lk[i].toDouble();
  if(c.width <= 0 || c.height <= 0 || !(c.intrinsics[0] > 0.) || !(c.intrinsics[1] > 0.)
     || !std::isfinite(c.intrinsics[2]) || !std::isfinite(c.intrinsics[3]))
    return std::nullopt;
  return c;
}

std::vector<float> convertLens(
    const std::vector<float>& d, const MeasuredCamera& from, const MeasuredCamera& to,
    const std::array<float, 16>& M)
{
  std::vector<float> out = d;
  const double R[3][3]{
      {M[0], M[4], M[8]}, {M[1], M[5], M[9]}, {M[2], M[6], M[10]}};
  for(std::size_t i = 0; i + 3 < d.size(); i += 4)
  {
    if(!(d[i + 3] > 0.5f))
      continue;
    std::array<double, 3> w{d[i], d[i + 1], d[i + 2]}, c;
    for(int r = 0; r < 3; r++)
      c[r] = R[0][r] * w[0] + R[1][r] * w[1] + R[2][r] * w[2];
    double px, py;
    if(!from.lens.project(from.intrinsics, c, px, py))
    {
      out[i + 3] = 0.f;
      continue;
    }
    const auto n = to.lens.unproject(to.intrinsics, px, py);
    for(int r = 0; r < 3; r++)
      out[i + r] = float(R[r][0] * n[0] + R[r][1] * n[1] + R[r][2] * n[2]);
  }
  return out;
}

namespace
{
std::vector<float> subsample(const std::vector<float>& m, int W, int H, int s, int& w, int& h)
{
  w = (W + s - 1) / s;
  h = (H + s - 1) / s;
  std::vector<float> out(std::size_t(w) * h * 4);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
      std::copy_n(&m[(std::size_t(y * s) * W + x * s) * 4], 4, &out[(std::size_t(y) * w + x) * 4]);
  return out;
}
}

LensEstimate estimateRadialDistortion(
    const std::vector<std::vector<float>>& maps, const std::vector<std::array<int, 2>>& sizes,
    const MeasuredCamera& measured, CameraLens::Model model,
    const std::array<double, 4>& K, const std::array<float, 16>& M)
{
  LensEstimate est;
  std::vector<std::vector<float>> small;
  std::vector<std::array<int, 2>> smallSize;
  for(std::size_t i = 0; i < maps.size() && i < sizes.size(); i++)
  {
    const int s = std::max(1, std::max(sizes[i][0], sizes[i][1]) / 480);
    int w{}, h{};
    small.push_back(subsample(maps[i], sizes[i][0], sizes[i][1], s, w, h));
    smallSize.push_back({w, h});
  }
  if(small.empty())
    return est;
  auto cost = [&](double k1) {
    MeasuredCamera to = measured;
    to.intrinsics = K;
    to.lens.model = model;
    to.lens.k = {};
    to.lens.k[0] = k1;
    double sum = 0.;
    int used = 0;
    for(std::size_t i = 0; i < small.size(); i++)
    {
      const auto conv = convertLens(small[i], measured, to, M);
      if(const double r = planeResidual(conv, smallSize[i][0], smallSize[i][1], 6., 2);
         r >= 0.)
      {
        sum += r;
        used++;
      }
    }
    return used ? sum / used : 1e9;
  };
  constexpr int scan = 10;
  constexpr double lo = -0.45, hi = 0.45, stepK = (hi - lo) / (scan - 1);
  int bestK = 0;
  double bestCost = 1e300;
  for(int i = 0; i < scan; i++)
    if(const double c = cost(lo + i * stepK); c < bestCost)
    {
      bestCost = c;
      bestK = i;
    }
  double a = lo + std::max(0, bestK - 1) * stepK, b = lo + std::min(scan - 1, bestK + 1) * stepK;
  const double g = (std::sqrt(5.) - 1.) / 2.;
  double c = b - g * (b - a), d = a + g * (b - a);
  double fc = cost(c), fd = cost(d);
  for(int it = 0; it < 12; it++)
  {
    if(fc < fd)
    {
      b = d;
      d = c;
      fd = fc;
      c = b - g * (b - a);
      fc = cost(c);
    }
    else
    {
      a = c;
      c = d;
      fc = fd;
      d = a + g * (b - a);
      fd = cost(d);
    }
  }
  est.k1 = (a + b) / 2.;
  est.residual = cost(est.k1);
  est.residualAtZero = cost(0.);
  est.ok = est.residual < 1e8;
  return est;
}
}
