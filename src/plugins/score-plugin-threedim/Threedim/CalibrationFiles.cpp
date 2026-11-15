#include "CalibrationFiles.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdlib>

namespace Threedim
{
namespace
{
std::vector<float> readRGB(const QString& path, int w, int h)
{
  if(!QFile::exists(path))
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

bool writeRGB(const QString& path, const std::vector<float>& rgb, int w, int h)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = 3;
  img.data = rgb;
  return score::gfx::writePFM(path, img);
}
}

CalibrationFile loadCalibration(const std::string& path)
{
  CalibrationFile c;
  c.base = SaveFloatTexture::basePath(path);
  const QString qbase = QString::fromStdString(c.base);
  QJsonObject o;
  if(QFile f{qbase + ".json"}; f.open(QIODevice::ReadOnly))
    o = QJsonDocument::fromJson(f.readAll()).object();
  const QString dir = QFileInfo(qbase + ".json").absolutePath() + '/';
  const auto listed = o["files"].toArray();
  auto file = [&](const char* suffix) -> QString {
    for(const auto& v : listed)
      if(const auto name = v.toString(); name.endsWith(QLatin1String(suffix), Qt::CaseInsensitive))
        return QFileInfo(name).isAbsolute() ? name : dir + name;
    return qbase + suffix;
  };

  QString err;
  const QString xyzFile = file(".xyz.pfm");
  const auto img = score::gfx::loadFloatImageRGBA(xyzFile, &err);
  if(!img.valid() || img.channels != 4)
  {
    c.error = "cannot read " + xyzFile.toStdString()
              + (err.isEmpty() ? std::string{} : ": " + err.toStdString());
    return c;
  }
  c.width = img.width;
  c.height = img.height;
  c.xyz = img.data;

  c.viewerMode = o["mode"].toString() == "viewer";
  const auto k = o["intrinsics"].toArray();
  for(int i = 0; i < 4 && i < k.size(); i++)
    c.intrinsics[i] = float(k[i].toDouble());
  const auto p = o["worldFromProjector"].toArray();
  if(p.size() == 16)
  {
    for(int i = 0; i < 16; i++)
      c.pose[i] = float(p[i].toDouble());
    c.hasPose = true;
  }
  c.lum = readRGB(file(".lum.pfm"), c.width, c.height);
  c.black = readRGB(file(".black.pfm"), c.width, c.height);
  return c;
}

bool saveCalibration(const CalibrationFile& c, const std::string& copyJsonFrom)
{
  if(!c.ok() || c.base.empty())
    return false;
  const QString qbase = QString::fromStdString(c.base);
  score::gfx::FloatImage img;
  img.width = c.width;
  img.height = c.height;
  img.channels = 4;
  img.data = c.xyz;
  if(!score::gfx::writeFloatImageSplit(qbase, img))
    return false;
  QJsonArray files{
      QFileInfo(qbase + ".xyz.pfm").fileName(), QFileInfo(qbase + ".valid.pfm").fileName()};
  if(!c.lum.empty() && writeRGB(qbase + ".lum.pfm", c.lum, c.width, c.height))
    files.append(QFileInfo(qbase + ".lum.pfm").fileName());
  if(!c.black.empty() && writeRGB(qbase + ".black.pfm", c.black, c.width, c.height))
    files.append(QFileInfo(qbase + ".black.pfm").fileName());

  QJsonObject o;
  if(!copyJsonFrom.empty())
  {
    QFile src{QString::fromStdString(SaveFloatTexture::basePath(copyJsonFrom)) + ".json"};
    if(src.open(QIODevice::ReadOnly))
      o = QJsonDocument::fromJson(src.readAll()).object();
  }
  o["mode"] = c.viewerMode ? "viewer" : "depth";
  o["width"] = c.width;
  o["height"] = c.height;
  QJsonArray k, pose;
  for(float v : c.intrinsics)
    k.append(v);
  for(float v : c.pose)
    pose.append(v);
  o["intrinsics"] = k;
  o["worldFromProjector"] = pose;
  o["files"] = files;
  QFile f{qbase + ".json"};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(QJsonDocument(o).toJson());
  return true;
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

std::vector<float> splitNumbers(std::string_view list)
{
  std::vector<float> out;
  std::string cur;
  auto flush = [&] {
    if(!cur.empty())
    {
      char* end{};
      const float v = std::strtof(cur.c_str(), &end);
      if(end != cur.c_str())
        out.push_back(v);
    }
    cur.clear();
  };
  for(char ch : list)
  {
    if(ch == ',' || ch == ';' || ch == ' ' || ch == '\n' || ch == '\t')
      flush();
    else
      cur.push_back(ch);
  }
  flush();
  return out;
}
}
