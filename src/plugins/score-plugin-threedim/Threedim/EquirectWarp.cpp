#include "EquirectWarp.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cmath>

namespace Threedim
{
EquirectWarp equirectWarp(
    const std::vector<float>& d, int width, int height, QRectF src)
{
  EquirectWarp w;
  w.width = width;
  w.height = height;
  const std::size_t n = std::size_t(width) * height;
  w.uv.assign(n * 2, 0.f);
  w.valid.assign(n, 0.f);
  if(d.size() < n * 4 || src.width() <= 0. || src.height() <= 0.)
    return w;

  auto dir = [&](std::size_t i, double& x, double& y, double& z) {
    const float* p = &d[i * 4];
    if(!(p[3] > 0.5f) || !std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
      return false;
    const double l = std::sqrt(double(p[0]) * p[0] + double(p[1]) * p[1] + double(p[2]) * p[2]);
    if(!(l > 1e-9))
      return false;
    x = p[0] / l;
    y = p[1] / l;
    z = p[2] / l;
    return true;
  };

  double s = 0., c = 0.;
  for(std::size_t i = 0; i < n; i++)
    if(double x, y, z; dir(i, x, y, z))
    {
      const double lon = std::atan2(x, -z);
      s += std::sin(lon);
      c += std::cos(lon);
    }
  const double ref = (s != 0. || c != 0.) ? std::atan2(s, c) : 0.;

  for(std::size_t i = 0; i < n; i++)
  {
    double x, y, z;
    if(!dir(i, x, y, z))
      continue;
    double dl = std::atan2(x, -z) - ref;
    dl -= 2. * M_PI * std::floor((dl + M_PI) / (2. * M_PI));
    const double lon = ref + dl;
    const double lat = std::asin(std::clamp(y, -1., 1.));
    const double u = lon / (2. * M_PI) + 0.5;
    const double v = 0.5 - lat / M_PI;
    w.uv[i * 2] = float((u - src.x()) / src.width());
    w.uv[i * 2 + 1] = float((v - src.y()) / src.height());
    w.valid[i] = 1.f;
  }
  return w;
}

namespace
{
bool listInJson(const QString& base, const QStringList& names, QString* error)
{
  QFile f{base + QStringLiteral(".json")};
  if(!f.open(QIODevice::ReadOnly))
    return true;
  auto obj = QJsonDocument::fromJson(f.readAll()).object();
  f.close();
  auto files = obj.value(QStringLiteral("files")).toArray();
  for(const QString& name : names)
    if(!files.contains(name))
      files.append(name);
  obj.insert(QStringLiteral("files"), files);
  QSaveFile out{base + QStringLiteral(".json")};
  if(!out.open(QIODevice::WriteOnly))
  {
    if(error)
      *error = QStringLiteral("cannot write %1.json").arg(base);
    return false;
  }
  out.write(QJsonDocument{obj}.toJson());
  return out.commit();
}
}

bool savePointsMap(
    const QString& base, const std::vector<float>& points, int width, int height,
    QString* error)
{
  const std::size_t n = std::size_t(width) * height;
  if(width <= 0 || height <= 0 || points.size() < n * 4)
  {
    if(error)
      *error = QStringLiteral("empty points map");
    return false;
  }
  score::gfx::FloatImage xyz, valid;
  xyz.width = valid.width = width;
  xyz.height = valid.height = height;
  xyz.channels = 3;
  valid.channels = 1;
  xyz.data.resize(n * 3);
  valid.data.resize(n);
  for(std::size_t i = 0; i < n; i++)
  {
    const float* p = &points[i * 4];
    const bool ok = p[3] > 0.5f && std::isfinite(p[0]) && std::isfinite(p[1])
                    && std::isfinite(p[2]);
    for(int c = 0; c < 3; c++)
      xyz.data[i * 3 + c] = ok ? p[c] : 0.f;
    valid.data[i] = ok ? 1.f : 0.f;
  }
  const QString map = base + QStringLiteral(".points.pfm");
  const QString mask = score::gfx::validMapPath(map);
  if(!score::gfx::writePFM(map, xyz, error) || !score::gfx::writePFM(mask, valid, error))
    return false;
  return listInJson(base, {QFileInfo{map}.fileName(), QFileInfo{mask}.fileName()}, error);
}

bool saveEquirectWarp(const QString& base, const EquirectWarp& w, QString* error)
{
  const std::size_t n = std::size_t(w.width) * w.height;
  if(w.width <= 0 || w.height <= 0 || w.uv.size() < n * 2 || w.valid.size() < n)
  {
    if(error)
      *error = QStringLiteral("empty warp");
    return false;
  }
  score::gfx::FloatImage uv;
  uv.width = w.width;
  uv.height = w.height;
  uv.channels = 3;
  uv.data.assign(n * 3, 0.f);
  for(std::size_t i = 0; i < n; i++)
  {
    uv.data[i * 3] = w.uv[i * 2];
    uv.data[i * 3 + 1] = w.uv[i * 2 + 1];
  }
  score::gfx::FloatImage valid;
  valid.width = w.width;
  valid.height = w.height;
  valid.channels = 1;
  valid.data = w.valid;
  const QString map = base + QStringLiteral(".warp.pfm");
  const QString mask = score::gfx::validMapPath(map);
  if(!score::gfx::writePFM(map, uv, error) || !score::gfx::writePFM(mask, valid, error))
    return false;

  QFile f{base + QStringLiteral(".json")};
  if(!f.open(QIODevice::ReadOnly))
    return true;
  auto obj = QJsonDocument::fromJson(f.readAll()).object();
  f.close();
  auto files = obj.value(QStringLiteral("files")).toArray();
  for(const QString& name : {QFileInfo{map}.fileName(), QFileInfo{mask}.fileName()})
    if(!files.contains(name))
      files.append(name);
  obj.insert(QStringLiteral("files"), files);
  QSaveFile out{base + QStringLiteral(".json")};
  if(!out.open(QIODevice::WriteOnly))
  {
    if(error)
      *error = QStringLiteral("cannot write %1.json").arg(base);
    return false;
  }
  out.write(QJsonDocument{obj}.toJson());
  return out.commit();
}
}
