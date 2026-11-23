#include "CalibrationFiles.hpp"

#include <Gfx/Window/CalibrationSet.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

#include <cmath>
#include <cstdlib>

namespace Threedim
{
namespace
{
void fail(QString* error, const QString& e)
{
  if(error)
    *error = e;
}

QString fileName(const QString& path)
{
  return QFileInfo{path}.fileName();
}

QStringList writtenFiles(const QString& base, const CalibrationMap& m)
{
  if(m.image.channels == 4 && m.suffix.compare(".xyz.pfm", Qt::CaseInsensitive) == 0)
    return {base + QStringLiteral(".xyz.pfm"), base + QStringLiteral(".valid.pfm")};
  return {base + m.suffix};
}

}

bool writeCalibrationMap(const QString& base, const CalibrationMap& m, QString* error)
{
  const auto& img = m.image;
  if(!img.valid())
  {
    fail(error, QStringLiteral("empty map %1").arg(base + m.suffix));
    return false;
  }
  if(img.channels == 4)
  {
    if(m.suffix.compare(".xyz.pfm", Qt::CaseInsensitive) != 0)
    {
      fail(error, QStringLiteral("a 4-channel map is an .xyz.pfm"));
      return false;
    }
    return score::gfx::writeFloatImageSplit(base, img, error);
  }
  return score::gfx::writePFM(base + m.suffix, img, error);
}

namespace
{
std::vector<float> readRGB(const QString& path, int w, int h)
{
  if(path.isEmpty())
    return {};
  const auto img = score::gfx::readPFM(path);
  if(!img.valid() || img.width != w || img.height != h)
    return {};
  std::vector<float> out(std::size_t(w) * h * 3);
  for(std::size_t i = 0; i < std::size_t(w) * h; i++)
    for(int c = 0; c < 3; c++)
      out[i * 3 + c] = img.data[i * img.channels + (img.channels == 1 ? 0 : c)];
  return out;
}

score::gfx::FloatImage image(const std::vector<float>& data, int w, int h, int channels)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = channels;
  img.data = data;
  return img;
}
}

bool writeJsonFile(const QString& path, const QJsonObject& o, QString* error)
{
  QSaveFile f{path};
  const QByteArray bytes = QJsonDocument(o).toJson();
  if(!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
  {
    fail(error, QStringLiteral("cannot write %1").arg(path));
    return false;
  }
  return true;
}

QJsonObject readJsonFile(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(f.readAll()).object();
}

bool writeCalibration(const CalibrationWrite& w, QString* error)
{
  if(w.base.isEmpty())
  {
    fail(error, QStringLiteral("no file name"));
    return false;
  }
  const QString jsonPath = w.base + QStringLiteral(".json");
  const bool exists = QFileInfo{jsonPath}.isFile();
  if(exists && w.createOnly)
  {
    fail(error, QStringLiteral("%1 exists").arg(jsonPath));
    return false;
  }
  QJsonObject o = exists ? readJsonFile(jsonPath) : QJsonObject{};
  for(const auto& m : w.maps)
  {
    const int ow = o.value("width").toInt(), oh = o.value("height").toInt();
    if(ow > 0 && oh > 0 && (m.image.width != ow || m.image.height != oh))
    {
      fail(
          error, QStringLiteral("%1 is a %2 x %3 projector, the map is %4 x %5")
                     .arg(jsonPath)
                     .arg(ow)
                     .arg(oh)
                     .arg(m.image.width)
                     .arg(m.image.height));
      return false;
    }
  }

  QStringList files;
  for(const auto& m : w.maps)
  {
    if(!writeCalibrationMap(w.base, m, error))
      return false;
    for(const auto& f : writtenFiles(w.base, m))
      files.push_back(fileName(f));
  }
  for(const auto& v : o.value("files").toArray())
  {
    const QString name = v.toString();
    if(files.contains(name, Qt::CaseInsensitive))
      continue;
    for(const auto& s : w.keepListed)
      if(name.endsWith(s, Qt::CaseInsensitive))
      {
        files.push_back(name);
        break;
      }
  }

  for(auto it = w.fields.begin(); it != w.fields.end(); ++it)
    o[it.key()] = it.value();
  if(!w.maps.empty())
  {
    o["width"] = w.maps.front().image.width;
    o["height"] = w.maps.front().image.height;
  }
  o["files"] = QJsonArray::fromStringList(files);
  return writeJsonFile(jsonPath, o, error);
}

QString calibrationMapFile(const QString& base, const QString& suffix)
{
  if(base.isEmpty())
    return {};
  if(const auto listing = Gfx::calibrationListing(base))
  {
    const QString dir = QFileInfo{base + QStringLiteral(".json")}.absolutePath();
    for(const auto& name : *listing)
      if(name.endsWith(suffix, Qt::CaseInsensitive))
      {
        const QString path = QFileInfo{name}.isAbsolute() ? name : dir + '/' + name;
        return QFileInfo{path}.isFile() ? path : QString{};
      }
    return {};
  }
  const QString path = base + suffix;
  return QFileInfo{path}.isFile() ? path : QString{};
}

CalibrationFile loadCalibration(const QString& path)
{
  CalibrationFile c;
  c.base = Gfx::calibrationBase(path);
  if(c.base.isEmpty())
  {
    c.error = QStringLiteral("no calibration file");
    return c;
  }
  c.json = readJsonFile(c.base + QStringLiteral(".json"));

  const QString xyzFile = calibrationMapFile(c.base, QStringLiteral(".xyz.pfm"));
  if(xyzFile.isEmpty())
  {
    c.error = QStringLiteral("no XYZ map for %1").arg(c.base);
    return c;
  }
  QString err;
  const auto xyz = score::gfx::readPFM(xyzFile, &err);
  if(!xyz.valid() || xyz.channels != 3)
  {
    c.error = QStringLiteral("cannot read %1%2")
                  .arg(xyzFile, err.isEmpty() ? QString{} : ": " + err);
    return c;
  }
  auto rgba = score::gfx::toRGBA(xyz);
  const QString validFile = calibrationMapFile(c.base, QStringLiteral(".valid.pfm"));
  if(!validFile.isEmpty())
  {
    const auto valid = score::gfx::readPFM(validFile);
    if(!valid.valid() || valid.width != xyz.width || valid.height != xyz.height)
    {
      c.error = QStringLiteral("%1 does not match %2").arg(validFile, xyzFile);
      return c;
    }
    for(std::size_t i = 0; i < std::size_t(xyz.width) * xyz.height; i++)
      rgba.data[i * 4 + 3] = valid.data[i * valid.channels];
  }
  const int jw = c.json.value("width").toInt(), jh = c.json.value("height").toInt();
  if(jw > 0 && jh > 0 && (jw != xyz.width || jh != xyz.height))
  {
    c.error = QStringLiteral("%1 is %2 x %3, %4.json says %5 x %6")
                  .arg(xyzFile)
                  .arg(xyz.width)
                  .arg(xyz.height)
                  .arg(c.base)
                  .arg(jw)
                  .arg(jh);
    return c;
  }
  c.width = xyz.width;
  c.height = xyz.height;
  c.xyz = std::move(rgba.data);

  c.viewerMode = c.json.value("mode").toString() == "viewer";
  const auto k = c.json.value("intrinsics").toArray();
  for(int i = 0; i < 4 && i < k.size(); i++)
    c.intrinsics[i] = float(k[i].toDouble());
  const auto p = c.json.value("worldFromProjector").toArray();
  if(p.size() == 16)
  {
    for(int i = 0; i < 16; i++)
      c.pose[i] = float(p[i].toDouble());
    c.hasPose = true;
  }
  c.lum = readRGB(calibrationMapFile(c.base, QStringLiteral(".lum.pfm")), c.width, c.height);
  c.black = readRGB(
      calibrationMapFile(c.base, QStringLiteral(".black.pfm")), c.width, c.height);
  return c;
}

bool saveCalibration(const CalibrationFile& c, QString* error)
{
  if(!c.ok())
  {
    fail(error, QStringLiteral("no XYZ map to save"));
    return false;
  }
  CalibrationWrite w;
  w.base = c.base;
  w.maps.push_back({QStringLiteral(".xyz.pfm"), image(c.xyz, c.width, c.height, 4)});
  if(!c.lum.empty())
    w.maps.push_back({QStringLiteral(".lum.pfm"), image(c.lum, c.width, c.height, 3)});
  if(!c.black.empty())
    w.maps.push_back({QStringLiteral(".black.pfm"), image(c.black, c.width, c.height, 3)});
  w.fields = c.json;
  w.fields.remove("files");
  w.fields["mode"] = c.viewerMode ? "viewer" : c.json.value("mode").toString("depth");
  QJsonArray k, pose;
  for(float v : c.intrinsics)
    k.append(v);
  w.fields["intrinsics"] = k;
  if(c.hasPose)
  {
    for(float v : c.pose)
      pose.append(v);
    w.fields["worldFromProjector"] = pose;
  }
  w.keepListed = {QStringLiteral(".lum.pfm"), QStringLiteral(".black.pfm")};
  return writeCalibration(w, error);
}

QString calibrationSetRenamed(const QString& base, const QString& tag)
{
  static const QRegularExpression index{QStringLiteral("_p[0-9]+$")};
  if(const auto m = index.match(base); m.hasMatch())
    return base.left(m.capturedStart()) + tag + m.captured();
  return base + tag;
}

std::vector<std::string> splitPaths(std::string_view list)
{
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&] {
    while(!cur.empty() && (cur.back() == ' ' || cur.back() == '\t' || cur.back() == '\r'))
      cur.pop_back();
    std::size_t b = 0;
    while(b < cur.size() && (cur[b] == ' ' || cur[b] == '\t'))
      b++;
    if(b < cur.size())
      out.push_back(cur.substr(b));
    cur.clear();
  };
  for(char ch : list)
  {
    if(ch == '\n' || ch == ';' || ch == '|')
      flush();
    else
      cur.push_back(ch);
  }
  flush();
  return out;
}

std::optional<std::vector<float>> parseNumbers(std::string_view list)
{
  std::vector<float> out;
  std::string cur;
  bool ok = true;
  auto flush = [&] {
    if(cur.empty())
      return;
    char* end{};
    const float v = std::strtof(cur.c_str(), &end);
    if(end != cur.c_str() + cur.size() || !std::isfinite(v))
      ok = false;
    else
      out.push_back(v);
    cur.clear();
  };
  for(char ch : list)
  {
    if(ch == ',' || ch == ';' || ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r')
      flush();
    else
      cur.push_back(ch);
  }
  flush();
  if(!ok)
    return std::nullopt;
  return out;
}
}
