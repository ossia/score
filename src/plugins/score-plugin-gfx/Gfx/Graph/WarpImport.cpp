#include <Gfx/Graph/WarpImport.hpp>

#include <score/tools/Zip.hpp>

#include <QFile>
#include <QImage>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <limits>

namespace score::gfx
{

namespace
{
void warpSetError(QString* error, const QString& e)
{
  if(error)
    *error = e;
}

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
  QImage img = QImage::fromData(bytes);
  if(img.isNull())
    return {};
  img = img.convertToFormat(QImage::Format_RGBA32FPx4);
  FloatImage out;
  out.width = img.width();
  out.height = img.height();
  out.channels = 4;
  out.data.resize(std::size_t(out.width) * out.height * 4);
  for(int y = 0; y < out.height; y++)
    std::copy_n(
        reinterpret_cast<const float*>(img.constScanLine(y)), out.width * 4, out.row(y));
  return out;
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
    warpSetError(error, QStringLiteral("truncated warp mesh"));
    return res;
  }
  bool ok1{}, ok2{}, ok3{};
  const int type = tok[0].toInt(&ok1);
  const int nx = tok[1].toInt(&ok2);
  const int ny = tok[2].toInt(&ok3);
  if(!ok1 || !ok2 || !ok3 || nx < 2 || ny < 2 || type != 2
     || tok.size() < 3 + qsizetype(nx) * ny * 5)
  {
    warpSetError(error, QStringLiteral("not a rectangular Bourke warp mesh"));
    return res;
  }

  std::vector<MeshNode> nodes(std::size_t(nx) * ny);
  double maxX = 0.;
  for(std::size_t k = 0; k < nodes.size(); k++)
  {
    auto num = [&](int f) { return tok[3 + qsizetype(k) * 5 + f].toDouble(); };
    nodes[k] = {num(0), num(1), num(2), num(3), num(4)};
    maxX = std::max(maxX, std::abs(nodes[k].x));
  }
  const double aspect = maxX > 0. ? maxX : 1.;
  if(width <= 0 || height <= 0)
  {
    width = 1024;
    height = std::max(1, int(std::lround(1024. / aspect)));
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

  auto toPixel = [&](const MeshNode& n) {
    return std::pair<double, double>{
        (n.x / aspect + 1.) * 0.5 * width, (1. - n.y) * 0.5 * height};
  };

  auto raster = [&](const MeshNode& a, const MeshNode& b, const MeshNode& c) {
    if(a.i < 0. || b.i < 0. || c.i < 0.)
      return;
    const auto [ax, ay] = toPixel(a);
    const auto [bx, by] = toPixel(b);
    const auto [cx, cy] = toPixel(c);
    const double area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if(std::abs(area) < 1e-12)
      return;
    const int x0 = std::max(0, int(std::floor(std::min({ax, bx, cx}))));
    const int x1 = std::min(width - 1, int(std::ceil(std::max({ax, bx, cx}))));
    const int y0 = std::max(0, int(std::floor(std::min({ay, by, cy}))));
    const int y1 = std::min(height - 1, int(std::ceil(std::max({ay, by, cy}))));
    for(int y = y0; y <= y1; y++)
      for(int x = x0; x <= x1; x++)
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
  };

  for(int j = 0; j + 1 < ny; j++)
    for(int i = 0; i + 1 < nx; i++)
    {
      const auto& n00 = nodes[std::size_t(j) * nx + i];
      const auto& n10 = nodes[std::size_t(j) * nx + i + 1];
      const auto& n01 = nodes[std::size_t(j + 1) * nx + i];
      const auto& n11 = nodes[std::size_t(j + 1) * nx + i + 1];
      raster(n00, n10, n11);
      raster(n00, n11, n01);
    }
  return res;
}

WarpImport loadBourkeMesh(const QString& path, int width, int height, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    warpSetError(error, QStringLiteral("cannot open %1").arg(path));
    return {};
  }
  return importBourkeMesh(f.readAll(), width, height, error);
}

std::vector<WarpImport> loadMPCDI(const QString& path, QString* error)
{
  std::vector<WarpImport> out;
  QString err;
  const QByteArray xml = score::readZipMember(path, QStringLiteral("mpcdi.xml"), err);
  if(xml.isEmpty())
  {
    warpSetError(error, err.isEmpty() ? QStringLiteral("no mpcdi.xml in %1").arg(path) : err);
    return out;
  }

  struct Region
  {
    QString id;
    int w{}, h{};
  };
  struct MapFile
  {
    QString path;
    double gamma{1.};
    bool topLeft{false};
  };
  struct FileSet
  {
    QString region;
    MapFile warp, alpha, beta;
  };
  std::vector<Region> regions;
  std::vector<FileSet> filesets;
  QString profile;

  QXmlStreamReader r{xml};
  FileSet* cur{};
  MapFile* curMap{};
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
        cur = &filesets.back();
      }
      else if(cur && name == QLatin1String("geometryWarpFile"))
        curMap = &cur->warp;
      else if(cur && name == QLatin1String("alphaMap"))
        curMap = &cur->alpha;
      else if(cur && name == QLatin1String("betaMap"))
        curMap = &cur->beta;
      else if(curMap && name == QLatin1String("path"))
        curMap->path = r.readElementText().trimmed();
      else if(curMap && name == QLatin1String("gammaEmbedded"))
        curMap->gamma = r.readElementText().trimmed().toDouble();
      else if(curMap && name == QLatin1String("originOf2DData"))
        curMap->topLeft = r.readElementText().trimmed().compare(
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
        cur = nullptr;
    }
  }
  if(r.hasError())
  {
    warpSetError(error, QStringLiteral("mpcdi.xml: %1").arg(r.errorString()));
    return out;
  }

  const bool threeD = profile == QLatin1String("3d") || profile == QLatin1String("a3")
                      || profile == QLatin1String("sl");
  for(const auto& fs : filesets)
  {
    WarpImport w;
    w.region = fs.region;
    w.threeD = threeD;
    for(const auto& reg : regions)
      if(reg.id == fs.region)
      {
        w.width = reg.w;
        w.height = reg.h;
      }

    auto member = [&](const MapFile& m) -> FloatImage {
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
          float* d = w.map.data.data() + k * 4;
          float v[3]{s[0], warp.channels > 1 ? s[1] : 0.f, warp.channels > 2 ? s[2] : 0.f};
          const bool ok = std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
          if(!ok)
            continue;
          if(threeD)
          {
            d[0] = v[0];
            d[1] = v[1];
            d[2] = v[2];
          }
          else
          {
            d[0] = v[0];
            d[1] = fs.warp.topLeft ? v[1] : 1.f - v[1];
          }
          d[3] = 1.f;
        }
      }
    }
    w.alpha = member(fs.alpha);
    if(w.alpha.valid())
      w.alpha = greyToRGBA(w.alpha, fs.alpha.gamma);
    w.beta = member(fs.beta);
    if(w.beta.valid())
      w.beta = greyToRGBA(w.beta, 1.);
    out.push_back(std::move(w));
  }
  if(out.empty())
    warpSetError(error, QStringLiteral("no fileset in %1").arg(path));
  return out;
}

FloatImage loadWarpMap(const QString& spec, WarpMapKind kind, QString* error)
{
  QString path = spec;
  QString region;
  if(const int hash = spec.lastIndexOf('#');
     hash > 0 && spec.left(hash).endsWith(QStringLiteral(".mpcdi"), Qt::CaseInsensitive))
  {
    path = spec.left(hash);
    region = spec.mid(hash + 1);
  }

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
    auto all = loadMPCDI(path, error);
    for(auto& w : all)
      if(region.isEmpty() || w.region == region)
        return pick(w);
    warpSetError(error, QStringLiteral("no region %1 in %2").arg(region, path));
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
