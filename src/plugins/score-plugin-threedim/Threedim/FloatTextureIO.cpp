#include "FloatTextureIO.hpp"

#include <Gfx/Graph/WarpImport.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstring>

namespace Threedim
{

score::gfx::FloatImage
LoadFloatTexture::load(std::string_view filename, std::string_view bytes)
{
  const QString path = QString::fromUtf8(filename.data(), filename.size());
  if(!path.isEmpty() && QFile::exists(path))
    return score::gfx::loadWarpMap(path, score::gfx::WarpMapKind::Map);
  if(!bytes.empty())
    return score::gfx::toRGBA(
        score::gfx::parsePFM(QByteArray::fromRawData(bytes.data(), bytes.size())));
  return {};
}

void LoadFloatTexture::operator()()
{
  if(!m_pending)
    return;
  auto img = std::move(m_pending);
  m_pending.reset();

  outputs.loaded.value = img->valid() && img->channels == 4;
  if(!outputs.loaded.value)
  {
    outputs.size.value = {0.f, 0.f};
    return;
  }
  outputs.texture.create(img->width, img->height);
  std::memcpy(
      outputs.texture.texture.bytes, img->data.data(), img->data.size() * sizeof(float));
  outputs.texture.upload();
  outputs.size.value = {float(img->width), float(img->height)};
}

std::string SaveFloatTexture::basePath(std::string path)
{
  auto strip = [&](std::string_view suffix) {
    if(path.size() > suffix.size()
       && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
      path.resize(path.size() - suffix.size());
      return true;
    }
    return false;
  };
  if(strip(".pfm") || strip(".PFM"))
    if(!strip(".xyz"))
      strip(".valid");
  strip(".json");
  return path;
}

score::gfx::FloatImage
SaveFloatTexture::extract(const float* rgba, int width, int height, Channels channels)
{
  score::gfx::FloatImage img;
  if(!rgba || width <= 0 || height <= 0)
    return img;
  img.width = width;
  img.height = height;
  img.channels = channels == Channels::RGBA ? 4 : channels == Channels::RGB ? 3 : 1;
  const std::size_t n = std::size_t(width) * height;
  img.data.resize(n * img.channels);
  for(std::size_t i = 0; i < n; i++)
    for(int c = 0; c < img.channels; c++)
      img.data[i * img.channels + c] = rgba[i * 4 + c];
  return img;
}

void SaveFloatTexture::operator()()
{
  if(!inputs.save)
    return;
  outputs.saved.value = false;

  const auto& tex = inputs.image.texture;
  const std::string base = basePath(inputs.file.value);
  if(base.empty() || !tex.bytes || tex.width <= 0 || tex.height <= 0)
    return;

  const auto img = extract(tex.bytes, tex.width, tex.height, inputs.channels.value);
  const QString qbase = QString::fromStdString(base);
  if(!score::gfx::writeFloatImageSplit(qbase, img))
    return;

  if(inputs.sidecar.value)
  {
    QJsonObject o;
    o["width"] = img.width;
    o["height"] = img.height;
    QJsonArray k;
    for(float v : inputs.intrinsics.value)
      k.append(v);
    o["intrinsics"] = k;
    QJsonArray pose;
    for(float v : inputs.pose.value)
      pose.append(v);
    o["worldFromProjector"] = pose;
    o["viewer"] = QJsonArray{
        inputs.viewer.value.x, inputs.viewer.value.y, inputs.viewer.value.z};
    o["rms"] = inputs.rms.value;
    QJsonArray files;
    if(img.channels == 4)
    {
      files.append(QFileInfo(qbase + ".xyz.pfm").fileName());
      files.append(QFileInfo(qbase + ".valid.pfm").fileName());
    }
    else
    {
      files.append(QFileInfo(qbase + ".pfm").fileName());
    }
    o["files"] = files;
    QFile f{qbase + ".json"};
    if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      f.write(QJsonDocument(o).toJson());
  }

  outputs.written.value = img.channels == 4 ? base + ".xyz.pfm" : base + ".pfm";
  outputs.saved.value = true;
}
}
