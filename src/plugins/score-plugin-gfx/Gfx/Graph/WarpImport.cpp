#include <Gfx/Graph/WarpImport.hpp>

#include <score/tools/Zip.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace score::gfx
{

namespace
{
struct MeshNode
{
  double x, y, u, v, i;
};

FloatImage greyToRGBA(const FloatImage& img, double gamma)
{
  FloatImage out = toRGBA(img);
  if(gamma != 1. && gamma > 0.)
    for(std::size_t k = 0; k < out.data.size(); k++)
      if(k % 4 != 3)
        out.data[k] = float(std::pow(std::max(0.f, out.data[k]), gamma));
  return out;
}

FloatImage decodeImageBytes(const QByteArray& bytes, const QString& name)
{
  if(name.endsWith(QStringLiteral(".pfm"), Qt::CaseInsensitive))
    return toRGBA(parsePFM(bytes));
  return toFloatImageRGBA(QImage::fromData(bytes));
}
}

WarpImport importBourkeMesh(const QByteArray& text, int width, int height, QString* error)
{
  WarpImport res;
  QList<QByteArray> tok;
  for(const auto& line : text.split('\n'))
  {
    const auto t = line.trimmed();
    if(t.isEmpty() || t.startsWith('#'))
      continue;
    for(const auto& w : t.simplified().split(' '))
      tok.push_back(w);
  }
  if(tok.size() < 3)
  {
    setFileError(error, QStringLiteral("truncated warp mesh"));
    return res;
  }
  constexpr int maxNodes = 4096;
  constexpr int maxSize = 16384;
  constexpr qsizetype maxPixels = qsizetype(8192) * 8192;
  bool ok1{}, ok2{}, ok3{};
  const int type = tok[0].toInt(&ok1);
  const int nx = tok[1].toInt(&ok2);
  const int ny = tok[2].toInt(&ok3);
  if(!ok1 || !ok2 || !ok3 || nx < 2 || ny < 2 || nx > maxNodes || ny > maxNodes
     || type != 2 || qsizetype(nx) * ny > (tok.size() - 3) / 5)
  {
    setFileError(error, QStringLiteral("not a rectangular Bourke warp mesh"));
    return res;
  }

  std::vector<MeshNode> nodes(std::size_t(nx) * ny);
  double maxX = 0.;
  for(std::size_t k = 0; k < nodes.size(); k++)
  {
    auto num = [&](int f) { return tok[3 + qsizetype(k) * 5 + f].toDouble(); };
    auto& n = nodes[k];
    n = {num(0), num(1), num(2), num(3), num(4)};
    if(!(std::abs(n.x) <= 1e6) || !(std::abs(n.y) <= 1e6) || !std::isfinite(n.u)
       || !std::isfinite(n.v) || !std::isfinite(n.i))
    {
      n = {0., 0., 0., 0., -1.};
      continue;
    }
    maxX = std::max(maxX, std::abs(n.x));
  }
  const double aspect = maxX > 0. ? std::clamp(maxX, 1e-3, 1e3) : 1.;
  if(width <= 0 || height <= 0)
  {
    width = 1024;
    height = int(std::clamp(std::round(1024. / aspect), 1., 4096.));
  }
  if(width > maxSize || height > maxSize || qsizetype(width) * height > maxPixels)
  {
    setFileError(error, QStringLiteral("warp mesh raster too large"));
    return res;
  }

  auto toPixel = [&](const MeshNode& n) {
    return std::pair<double, double>{
        (n.x / aspect + 1.) * 0.5 * width, (1. - n.y) * 0.5 * height};
  };

  struct Box
  {
    int x0, x1, y0, y1;
  };
  auto bounds = [&](double a, double b, double c, int size) {
    const double lo = std::clamp(std::floor(std::min({a, b, c})), 0., double(size));
    const double hi = std::clamp(std::ceil(std::max({a, b, c})), -1., double(size - 1));
    return std::pair<int, int>{int(lo), int(hi)};
  };
  auto box = [&](const MeshNode& a, const MeshNode& b, const MeshNode& c) -> Box {
    if(a.i < 0. || b.i < 0. || c.i < 0.)
      return {0, -1, 0, -1};
    const auto [ax, ay] = toPixel(a);
    const auto [bx, by] = toPixel(b);
    const auto [cx, cy] = toPixel(c);
    const auto [x0, x1] = bounds(ax, bx, cx, width);
    const auto [y0, y1] = bounds(ay, by, cy, height);
    return {x0, x1, y0, y1};
  };
  auto forEachTriangle = [&](auto&& f) {
    for(int j = 0; j + 1 < ny; j++)
      for(int i = 0; i + 1 < nx; i++)
      {
        const auto& n00 = nodes[std::size_t(j) * nx + i];
        const auto& n10 = nodes[std::size_t(j) * nx + i + 1];
        const auto& n01 = nodes[std::size_t(j + 1) * nx + i];
        const auto& n11 = nodes[std::size_t(j + 1) * nx + i + 1];
        f(n00, n10, n11);
        f(n00, n11, n01);
      }
  };

  double work = 0.;
  forEachTriangle([&](const MeshNode& a, const MeshNode& b, const MeshNode& c) {
    const Box bb = box(a, b, c);
    if(bb.x1 >= bb.x0 && bb.y1 >= bb.y0)
      work += double(bb.x1 - bb.x0 + 1) * (bb.y1 - bb.y0 + 1);
  });
  if(work > 64. * width * height)
  {
    setFileError(error, QStringLiteral("warp mesh folds over itself"));
    return res;
  }

  res.width = width;
  res.height = height;
  res.map.width = res.alpha.width = width;
  res.map.height = res.alpha.height = height;
  res.map.channels = res.alpha.channels = 4;
  res.map.data.assign(std::size_t(width) * height * 4, 0.f);
  res.alpha.data.assign(std::size_t(width) * height * 4, 0.f);
  for(std::size_t k = 3; k < res.alpha.data.size(); k += 4)
    res.alpha.data[k] = 1.f;

  forEachTriangle([&](const MeshNode& a, const MeshNode& b, const MeshNode& c) {
    const Box bb = box(a, b, c);
    if(bb.x1 < bb.x0 || bb.y1 < bb.y0)
      return;
    const auto [ax, ay] = toPixel(a);
    const auto [bx, by] = toPixel(b);
    const auto [cx, cy] = toPixel(c);
    const double area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if(!(std::abs(area) >= 1e-12))
      return;
    for(int y = bb.y0; y <= bb.y1; y++)
      for(int x = bb.x0; x <= bb.x1; x++)
      {
        const double px = x + 0.5, py = y + 0.5;
        const double w0 = ((bx - px) * (cy - py) - (cx - px) * (by - py)) / area;
        const double w1 = ((cx - px) * (ay - py) - (ax - px) * (cy - py)) / area;
        const double w2 = 1. - w0 - w1;
        constexpr double eps = -1e-9;
        if(w0 < eps || w1 < eps || w2 < eps)
          continue;
        float* m = res.map.row(y) + x * 4;
        m[0] = float(w0 * a.u + w1 * b.u + w2 * c.u);
        m[1] = float(1. - (w0 * a.v + w1 * b.v + w2 * c.v));
        m[2] = 0.f;
        m[3] = 1.f;
        const float in = float(w0 * a.i + w1 * b.i + w2 * c.i);
        float* al = res.alpha.row(y) + x * 4;
        al[0] = al[1] = al[2] = in;
      }
  });
  return res;
}

WarpImport loadBourkeMesh(const QString& path, int width, int height, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    setFileError(error, QStringLiteral("cannot open %1").arg(path));
    return {};
  }
  return importBourkeMesh(f.readAll(), width, height, error);
}

namespace
{
struct MPCDIMapFile
{
  QString path;
  double gamma{1.};
  bool topLeft{false};
};
struct MPCDIFileSet
{
  QString region;
  MPCDIMapFile warp, alpha, beta;
};
struct MPCDIRegion
{
  QString id;
  int w{}, h{};
};
struct MPCDIDescription
{
  std::vector<MPCDIRegion> regions;
  std::vector<MPCDIFileSet> filesets;
  bool threeD{};
};

std::optional<MPCDIDescription> readMPCDIDescription(const QString& path, QString* error)
{
  QString err;
  const QByteArray xml = score::readZipMember(path, QStringLiteral("mpcdi.xml"), err);
  if(xml.isEmpty())
  {
    setFileError(error, err.isEmpty() ? QStringLiteral("no mpcdi.xml in %1").arg(path) : err);
    return {};
  }

  using MapFile = MPCDIMapFile;
  using FileSet = MPCDIFileSet;
  MPCDIDescription d;
  auto& regions = d.regions;
  auto& filesets = d.filesets;
  QString profile;

  QXmlStreamReader r{xml};
  std::size_t cur = 0;
  bool inSet = false;
  MapFile FileSet::*curMap{};
  while(!r.atEnd())
  {
    const auto tt = r.readNext();
    if(tt == QXmlStreamReader::StartElement)
    {
      const auto name = r.name();
      const auto attrs = r.attributes();
      if(name == QLatin1String("MPCDI"))
        profile = attrs.value("profile").toString();
      else if(name == QLatin1String("region"))
        regions.push_back(
            {attrs.value("id").toString(), attrs.value("xResolution").toInt(),
             attrs.value("yResolution").toInt()});
      else if(name == QLatin1String("fileset"))
      {
        filesets.push_back({attrs.value("region").toString(), {}, {}, {}});
        cur = filesets.size() - 1;
        inSet = true;
        curMap = nullptr;
      }
      else if(inSet && name == QLatin1String("geometryWarpFile"))
        curMap = &FileSet::warp;
      else if(inSet && name == QLatin1String("alphaMap"))
        curMap = &FileSet::alpha;
      else if(inSet && name == QLatin1String("betaMap"))
        curMap = &FileSet::beta;
      else if(inSet && curMap && name == QLatin1String("path"))
        (filesets[cur].*curMap).path = r.readElementText().trimmed();
      else if(inSet && curMap && name == QLatin1String("gammaEmbedded"))
        (filesets[cur].*curMap).gamma = r.readElementText().trimmed().toDouble();
      else if(inSet && curMap && name == QLatin1String("originOf2DData"))
        (filesets[cur].*curMap).topLeft = r.readElementText().trimmed().compare(
                                              QLatin1String("topLeft"), Qt::CaseInsensitive)
                                          == 0;
    }
    else if(tt == QXmlStreamReader::EndElement)
    {
      const auto name = r.name();
      if(name == QLatin1String("geometryWarpFile") || name == QLatin1String("alphaMap")
         || name == QLatin1String("betaMap"))
        curMap = nullptr;
      else if(name == QLatin1String("fileset"))
      {
        inSet = false;
        curMap = nullptr;
      }
    }
  }
  if(r.hasError())
  {
    setFileError(error, QStringLiteral("mpcdi.xml: %1").arg(r.errorString()));
    return {};
  }
  d.threeD = profile == QLatin1String("3d") || profile == QLatin1String("a3")
             || profile == QLatin1String("sl");
  return d;
}

WarpImport decodeMPCDIFileSet(
    const QString& path, const MPCDIDescription& d, const MPCDIFileSet& fs)
{
  const bool threeD = d.threeD;
  WarpImport w;
  w.region = fs.region;
  w.threeD = threeD;
  for(const auto& reg : d.regions)
    if(reg.id == fs.region)
    {
      w.width = reg.w;
      w.height = reg.h;
    }

  auto member = [&](const MPCDIMapFile& m) -> FloatImage {
    if(m.path.isEmpty())
      return {};
    QString e;
    const QByteArray bytes = score::readZipMember(path, m.path, e);
    if(bytes.isEmpty())
      return {};
    return decodeImageBytes(bytes, m.path);
  };

  if(!fs.warp.path.isEmpty())
  {
    QString e;
    const QByteArray bytes = score::readZipMember(path, fs.warp.path, e);
    FloatImage warp = parsePFM(bytes);
    if(warp.valid())
    {
      w.map.width = warp.width;
      w.map.height = warp.height;
      w.map.channels = 4;
      w.map.data.assign(std::size_t(warp.width) * warp.height * 4, 0.f);
      const std::size_t n = std::size_t(warp.width) * warp.height;
      for(std::size_t k = 0; k < n; k++)
      {
        const float* s = warp.data.data() + k * warp.channels;
        float* dst = w.map.data.data() + k * 4;
        float v[3]{s[0], warp.channels > 1 ? s[1] : 0.f, warp.channels > 2 ? s[2] : 0.f};
        const bool ok = std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
        if(!ok)
          continue;
        if(threeD)
        {
          dst[0] = v[0];
          dst[1] = v[1];
          dst[2] = v[2];
        }
        else
        {
          dst[0] = v[0];
          dst[1] = fs.warp.topLeft ? v[1] : 1.f - v[1];
        }
        dst[3] = 1.f;
      }
    }
  }
  w.alpha = member(fs.alpha);
  if(w.alpha.valid())
    w.alpha = greyToRGBA(w.alpha, fs.alpha.gamma);
  w.beta = member(fs.beta);
  if(w.beta.valid())
    w.beta = greyToRGBA(w.beta, 1.);
  w.betaLinear = fs.beta.path.endsWith(QStringLiteral(".pfm"), Qt::CaseInsensitive);
  return w;
}
}

std::vector<WarpImport> loadMPCDI(const QString& path, QString* error)
{
  std::vector<WarpImport> out;
  const auto d = readMPCDIDescription(path, error);
  if(!d)
    return out;
  for(const auto& fs : d->filesets)
    out.push_back(decodeMPCDIFileSet(path, *d, fs));
  if(out.empty())
    setFileError(error, QStringLiteral("no fileset in %1").arg(path));
  return out;
}

std::optional<WarpImport>
loadMPCDIRegion(const QString& path, const QString& region, QString* error)
{
  const auto d = readMPCDIDescription(path, error);
  if(!d)
    return {};
  for(const auto& fs : d->filesets)
    if(region.isEmpty() || fs.region == region)
      return decodeMPCDIFileSet(path, *d, fs);
  setFileError(
      error, d->filesets.empty() ? QStringLiteral("no fileset in %1").arg(path)
                                 : QStringLiteral("no region %1 in %2").arg(region, path));
  return {};
}

QString mapArchivePath(const QString& spec)
{
  if(const int hash = spec.lastIndexOf('#');
     hash > 0 && spec.left(hash).endsWith(QStringLiteral(".mpcdi"), Qt::CaseInsensitive))
    return spec.left(hash);
  return spec;
}

QString mapFileStamp(const QString& spec)
{
  if(spec.isEmpty())
    return {};
  const QFileInfo fi{mapArchivePath(spec)};
  return spec + QLatin1Char('|')
         + (fi.exists() ? QString::number(fi.lastModified().toMSecsSinceEpoch())
                              + QLatin1Char(':') + QString::number(fi.size())
                        : QStringLiteral("-"));
}

FloatImage loadWarpMap(const QString& spec, WarpMapKind kind, QString* error)
{
  const QString path = mapArchivePath(spec);
  const QString region = path.size() < spec.size() ? spec.mid(path.size() + 1) : QString{};

  auto pick = [&](WarpImport& w) -> FloatImage {
    switch(kind)
    {
      case WarpMapKind::Map:
        return std::move(w.map);
      case WarpMapKind::Alpha:
        return std::move(w.alpha);
      case WarpMapKind::Beta:
        return std::move(w.beta);
    }
    return {};
  };

  if(path.endsWith(QStringLiteral(".mpcdi"), Qt::CaseInsensitive))
  {
    if(auto w = loadMPCDIRegion(path, region, error))
      return pick(*w);
    return {};
  }
  if(path.endsWith(QStringLiteral(".data"), Qt::CaseInsensitive))
  {
    auto w = loadBourkeMesh(path, 0, 0, error);
    return pick(w);
  }
  return loadFloatImageRGBA(path, error);
}
}
