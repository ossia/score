#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Gfx/Graph/OutputQuality.hpp>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace score::gfx
{
namespace
{
QString stripSuffix(QString base, std::initializer_list<const char*> suffixes)
{
  for(const char* s : suffixes)
    if(base.endsWith(QLatin1String(s), Qt::CaseInsensitive))
    {
      base.chop(int(std::strlen(s)));
      break;
    }
  return base;
}

ResponseTable parseCube1D(const QByteArray& bytes, QString* error)
{
  CubeFile cube = parseCubeFile(bytes, error);
  if(!cube.valid())
    return {};
  if(cube.size3D > 0)
  {
    setFileError(error, QStringLiteral("a 3D .cube is not a response curve"));
    return {};
  }
  ResponseTable t;
  t.size = cube.size1D;
  const auto& dmin = cube.domainMin;
  const auto& dmax = cube.domainMax;
  if(dmin == std::array<float, 3>{0.f, 0.f, 0.f} && dmax == std::array<float, 3>{1.f, 1.f, 1.f})
  {
    t.inverse = std::move(cube.rgb);
    return t;
  }
  const auto& v = cube.rgb;
  t.inverse.resize(v.size());
  for(int i = 0; i < t.size; i++)
    for(int c = 0; c < 3; c++)
    {
      const float x = float(i) / float(t.size - 1);
      const float f
          = std::clamp((x - dmin[c]) / (dmax[c] - dmin[c]), 0.f, 1.f) * float(t.size - 1);
      const int i0 = std::min(int(f), t.size - 2);
      const float a = f - i0;
      t.inverse[i * 3 + c] = v[i0 * 3 + c] * (1.f - a) + v[(i0 + 1) * 3 + c] * a;
    }
  return t;
}
}

ResponseTable loadResponseTable(const QString& path, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    setFileError(error, f.errorString());
    return {};
  }
  const QByteArray bytes = f.readAll();
  if(path.endsWith(QStringLiteral(".cube"), Qt::CaseInsensitive))
    return parseCube1D(bytes, error);

  QJsonParseError pe;
  const auto o = QJsonDocument::fromJson(bytes, &pe).object();
  if(pe.error != QJsonParseError::NoError)
  {
    setFileError(error, pe.errorString());
    return {};
  }
  const auto inv = o["inverse"].toArray();
  ResponseTable t;
  if(inv.size() != 3)
  {
    setFileError(error, QStringLiteral("no \"inverse\" table (3 arrays)"));
    return {};
  }
  t.size = int(inv[0].toArray().size());
  if(t.size < 2 || t.size > 65536 || inv[1].toArray().size() != t.size
     || inv[2].toArray().size() != t.size)
  {
    setFileError(error, QStringLiteral("the \"inverse\" arrays differ in size or exceed 65536"));
    return {};
  }
  t.inverse.resize(std::size_t(t.size) * 3);
  for(int c = 0; c < 3; c++)
  {
    const auto a = inv[c].toArray();
    for(int i = 0; i < t.size; i++)
      t.inverse[std::size_t(i) * 3 + c] = float(a[i].toDouble());
  }
  if(o.contains("mixing"))
  {
    const auto mo = o["mixing"].toObject();
    const auto axes = mo["axis"].toArray();
    const auto shown = mo["shown"].toArray();
    const qsizetype N = axes.size() == 3 ? axes[0].toArray().size() : 0;
    photometric::ColourMixing mix;
    if(N >= 2 && N <= 17 && shown.size() == N * N * N)
    {
      for(int c = 0; c < 3; c++)
        for(const auto& v : axes[c].toArray())
          mix.axis[c].push_back(v.toDouble());
      mix.shown.reserve(shown.size());
      for(const auto& v : shown)
      {
        const auto a = v.toArray();
        if(a.size() != 3)
          break;
        mix.shown.push_back({a[0].toDouble(), a[1].toDouble(), a[2].toDouble()});
      }
    }
    if(mix.wellFormed())
      t.mixing = photometric::MixingInverse::build(mix);
    else
      setFileError(error, QStringLiteral("ignored an invalid \"mixing\" table"));
  }
  return t;
}

bool writeProjectorResponse(const QString& base, const ProjectorResponseFile& r)
{
  static const char* names[4]{"red", "green", "blue", "grey"};
  QJsonObject o;
  o["type"] = "projectorResponse";
  o["version"] = 1;
  o["note"] = "normalised per channel: f(0) = 0, f(1) = 1, light = f(code); inverse[c][i] "
              "is the code giving light i / (size - 1)";
  o["linearisation"] = r.linearisation;
  if(r.additivityError >= 0.)
    o["additivityError"] = r.additivityError;
  if(r.mixErrorAdditive >= 0.)
    o["mixErrorAdditive"] = r.mixErrorAdditive;
  if(r.mixErrorModel >= 0.)
    o["mixErrorModel"] = r.mixErrorModel;
  if(!r.samples.empty())
  {
    QJsonArray sa;
    for(auto& [code, col] : r.samples)
      sa.append(QJsonObject{
          {"code", QJsonArray{code[0], code[1], code[2]}},
          {"camera", QJsonArray{col[0], col[1], col[2]}}});
    o["samples"] = sa;
  }
  if(r.mixing.valid())
  {
    QJsonObject mo;
    QJsonArray axes, shown;
    for(int c = 0; c < 3; c++)
    {
      QJsonArray a;
      for(double v : r.mixing.axis[c])
        a.append(v);
      axes.append(a);
    }
    for(auto& v : r.mixing.shown)
      shown.append(QJsonArray{v[0], v[1], v[2]});
    mo["note"] = "light shown, in full-primary units, for per-channel linear light on the "
                 "grid axis x axis x axis; shown red fastest";
    mo["axis"] = axes;
    mo["shown"] = shown;
    o["mixing"] = mo;
  }
  QJsonObject chans;
  QJsonArray gammas;
  for(int c = 0; c < 4; c++)
  {
    const auto& ch = r.channels[c];
    if(ch.x.empty())
      continue;
    QJsonObject co;
    QJsonArray x, m, fit;
    for(std::size_t k = 0; k < ch.x.size(); k++)
    {
      x.append(ch.x[k]);
      m.append(ch.measured[k]);
      fit.append(ch.fitted[k]);
    }
    co["x"] = x;
    co["measured"] = m;
    co["fitted"] = fit;
    co["gamma"] = ch.gamma;
    chans[names[c]] = co;
    if(c < 3)
      gammas.append(ch.gamma);
  }
  o["channels"] = chans;
  o["gamma"] = gammas;
  const int N = std::max(2, r.inverseSize);
  QJsonArray inv;
  std::vector<std::array<float, 3>> table(N);
  for(int c = 0; c < 3; c++)
  {
    QJsonArray a;
    for(int i = 0; i < N; i++)
    {
      const double t = double(i) / (N - 1);
      const double v = r.channels[c].x.empty() ? std::pow(t, 1. / 2.2)
                                               : r.channels[c].inverse(t);
      table[i][c] = float(v);
      a.append(v);
    }
    inv.append(a);
  }
  o["inverseSize"] = N;
  o["inverse"] = inv;

  QFile f{base + QStringLiteral(".response.json")};
  const QByteArray json = QJsonDocument(o).toJson();
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(json) != json.size()
     || !f.flush())
    return false;
  f.close();

  QFile cube{base + QStringLiteral(".response.cube")};
  if(!cube.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return false;
  QTextStream ts{&cube};
  ts << "TITLE \"projector inverse response\"\n";
  ts << "LUT_1D_SIZE " << N << "\n";
  ts << "DOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 1\n";
  ts.setRealNumberPrecision(7);
  for(int i = 0; i < N; i++)
    ts << table[i][0] << ' ' << table[i][1] << ' ' << table[i][2] << '\n';
  ts.flush();
  return ts.status() == QTextStream::Ok && cube.flush();
}

bool writeCompensation(
    const QString& base, const CompensationMaps& maps, const CompensationInfo& info)
{
  if(!maps.valid())
    return false;
  const std::size_t n = std::size_t(maps.width) * maps.height;
  QJsonArray rows;
  for(int k = 0; k < 3; k++)
  {
    FloatImage img;
    img.width = maps.width;
    img.height = maps.height;
    img.channels = 3;
    img.data.resize(n * 3);
    for(std::size_t i = 0; i < n; i++)
      for(int c = 0; c < 3; c++)
        img.data[i * 3 + c] = maps.rows[k].data[i * 4 + c];
    const QString file = base + QStringLiteral(".comp%1.pfm").arg(k);
    if(!writePFM(file, img))
      return false;
    rows.append(QFileInfo(file).fileName());
  }
  FloatImage off;
  off.width = maps.width;
  off.height = maps.height;
  off.channels = 3;
  off.data.resize(n * 3);
  for(std::size_t i = 0; i < n; i++)
    for(int k = 0; k < 3; k++)
      off.data[i * 3 + k] = maps.rows[k].data[i * 4 + 3];
  const QString offFile = base + QStringLiteral(".compoffset.pfm");
  if(!writePFM(offFile, off))
    return false;

  QJsonObject o;
  o["type"] = "radiometricCompensation";
  o["version"] = 1;
  o["note"] = "per output pixel, linear projector RGB P = M c + o from linear content c; "
              "compN.pfm holds row N of M (rgb = M_N0 M_N1 M_N2), compoffset.pfm holds o";
  o["width"] = maps.width;
  o["height"] = maps.height;
  o["rows"] = rows;
  o["offset"] = QFileInfo(offFile).fileName();
  o["target"] = QJsonArray{info.target[0], info.target[1], info.target[2]};
  o["scale"] = info.scale;
  o["strength"] = info.strength;
  o["percentile"] = info.percentile;
  o["inGamut"] = info.inGamut;
  o["black"] = info.black;
  if(info.mixing)
    o["mixing"] = true;
  QFile f{base + QStringLiteral(".comp.json")};
  const QByteArray json = QJsonDocument(o).toJson();
  return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(json) == json.size()
         && f.flush();
}

CompensationMaps loadCompensation(const QString& path, QString* error)
{
  QString base = stripSuffix(
      path, {".comp.json", ".compoffset.pfm", ".comp0.pfm", ".comp1.pfm", ".comp2.pfm", ".json"});
  const QString json = path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)
                           ? path
                           : base + QStringLiteral(".comp.json");
  QStringList rowFiles{
      base + QStringLiteral(".comp0.pfm"), base + QStringLiteral(".comp1.pfm"),
      base + QStringLiteral(".comp2.pfm")};
  QString offFile = base + QStringLiteral(".compoffset.pfm");
  QFile f{json};
  if(f.open(QIODevice::ReadOnly))
  {
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    const QString dir = QFileInfo(f.fileName()).absolutePath() + '/';
    const auto rows = o["rows"].toArray();
    if(rows.size() == 3)
      for(int k = 0; k < 3; k++)
        rowFiles[k] = dir + rows[k].toString();
    if(o.contains("offset"))
      offFile = dir + o["offset"].toString();
  }
  CompensationMaps m;
  FloatImage off;
  if(QFileInfo::exists(offFile))
    off = readPFM(offFile);
  for(int k = 0; k < 3; k++)
  {
    QString err;
    const FloatImage row = readPFM(rowFiles[k], &err);
    if(!row.valid() || row.channels != 3)
    {
      setFileError(error, QStringLiteral("cannot read %1: %2").arg(rowFiles[k], err));
      return {};
    }
    if(k == 0)
    {
      m.width = row.width;
      m.height = row.height;
      if(QFileInfo::exists(offFile)
         && !(off.valid() && off.width == m.width && off.height == m.height
              && off.channels == 3))
        qWarning().noquote()
            << QStringLiteral(
                   "loadCompensation: the offset %1 (%2x%3, %4 channels) does not match "
                   "the %5x%6 RGB rows and is ignored")
                   .arg(offFile)
                   .arg(off.width)
                   .arg(off.height)
                   .arg(off.channels)
                   .arg(m.width)
                   .arg(m.height);
    }
    else if(row.width != m.width || row.height != m.height)
    {
      setFileError(error, QStringLiteral("the compensation rows differ in size"));
      return {};
    }
    const bool hasOff
        = off.valid() && off.width == m.width && off.height == m.height && off.channels == 3;
    auto& out = m.rows[k];
    out.width = m.width;
    out.height = m.height;
    out.channels = 4;
    out.data.resize(std::size_t(m.width) * m.height * 4);
    for(std::size_t i = 0, n = std::size_t(m.width) * m.height; i < n; i++)
    {
      for(int c = 0; c < 3; c++)
        out.data[i * 4 + c] = row.data[i * 3 + c];
      out.data[i * 4 + 3] = hasOff ? off.data[i * 3 + k] : 0.f;
    }
  }
  return m;
}
}
