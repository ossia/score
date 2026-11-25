#include "FloatTextureIO.hpp"

#include <Gfx/Graph/WarpImport.hpp>
#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QFileInfo>
#include <QImage>
#include <QJsonArray>

#include <cstring>

namespace Threedim
{

LoadFloatTexture::Loaded
LoadFloatTexture::load(const QString& filename, const QByteArray& bytes)
{
  Loaded l;
  QString err;
  const auto is = [&](const char* ext) {
    return filename.endsWith(QLatin1String(ext), Qt::CaseInsensitive);
  };
  if(is(".mpcdi"))
    l.image = score::gfx::loadWarpMap(filename, score::gfx::WarpMapKind::Map, &err);
  else if(is(".data"))
    l.image = score::gfx::importBourkeMesh(bytes, 0, 0, &err).map;
  else if(is(".pfm") || (filename.isEmpty() && bytes.startsWith("P")))
  {
    const auto pfm = score::gfx::parsePFM(bytes, &err);
    if(pfm.valid())
    {
      l.image = score::gfx::toRGBA(pfm);
      const QString valid = filename.isEmpty() ? QString{} : score::gfx::validMapPath(filename);
      if(pfm.channels == 3 && valid != filename && QFileInfo{valid}.isFile())
      {
        const auto v = score::gfx::readPFM(valid);
        if(v.valid() && v.width == pfm.width && v.height == pfm.height)
          for(std::size_t i = 0; i < std::size_t(pfm.width) * pfm.height; i++)
            l.image.data[i * 4 + 3] = v.data[i * v.channels];
      }
    }
  }
  else if(QImage img = QImage::fromData(bytes); !img.isNull())
    l.image = score::gfx::toFloatImageRGBA(img);
  else
    err = QStringLiteral("not an image");

  if(!l.image.valid() || l.image.channels != 4)
  {
    l.image = {};
    l.status = "Failed: cannot read " + filename.toStdString()
               + (err.isEmpty() ? std::string{} : ": " + err.toStdString());
  }
  else
    l.status = "Loaded " + std::to_string(l.image.width) + " x "
               + std::to_string(l.image.height);
  return l;
}

void LoadFloatTexture::worker::work(
    std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job)
{
  auto res = load(job->filename, job->bytes);
  if(auto j = jobs.lock())
    j->finish(id, std::move(res));
}

std::function<void(LoadFloatTexture&)> LoadFloatTexture::ins::map_t::process(file_type data)
{
  Job job{
      QString::fromUtf8(data.filename.data(), data.filename.size()),
      QByteArray(data.bytes.data(), qsizetype(data.bytes.size()))};
  return [job = std::move(job)](LoadFloatTexture& self) mutable {
    self.request(std::move(job));
  };
}

LoadFloatTexture::~LoadFloatTexture()
{
  if(node_state)
    node_state->release(this);
}

LoadFloatTexture::Jobs& LoadFloatTexture::jobs()
{
  if(!node_state)
    node_state = std::make_shared<Jobs>();
  return *node_state;
}

void LoadFloatTexture::request(Job job)
{
  auto& j = jobs();
  if(!j.claim(this))
    return;
  outputs.status.value = "Loading";
  worker.request(node_state, j.start(), std::make_shared<const Job>(std::move(job)));
}

void LoadFloatTexture::operator()()
{
  auto r = jobs().fresh(m_seen);
  if(!r)
    return;
  outputs.status.value = r->status;
  outputs.loaded.value = r->image.valid();
  if(!outputs.loaded.value)
  {
    outputs.size.value = {0.f, 0.f};
    return;
  }
  const auto& img = r->image;
  outputs.texture.create(img.width, img.height);
  std::memcpy(
      outputs.texture.texture.bytes, img.data.data(), img.data.size() * sizeof(float));
  outputs.texture.upload();
  outputs.size.value = {float(img.width), float(img.height)};
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

SaveFloatTexture::Result SaveFloatTexture::write(const Job& job)
{
  Result r;
  const QString base = Gfx::calibrationBase(job.path);
  if(base.isEmpty())
  {
    r.status = "Failed: no file";
    return r;
  }
  const bool rgba = job.image.channels == 4;
  CalibrationWrite w;
  w.base = base;
  w.maps.push_back({rgba ? QStringLiteral(".xyz.pfm") : QStringLiteral(".pfm"), job.image});
  const QString written = base + w.maps.front().suffix;

  QString err;
  if(job.sidecar)
  {
    const auto& k = job.intrinsics;
    if(k[0] > 0.f && k[1] > 0.f)
    {
      QJsonArray ka, pose;
      for(float v : k)
        ka.append(v);
      for(float v : job.pose)
        pose.append(v);
      w.fields["intrinsics"] = ka;
      w.fields["worldFromProjector"] = pose;
      w.fields["viewer"] = QJsonArray{job.viewer[0], job.viewer[1], job.viewer[2]};
      w.fields["rms"] = job.rms;
    }
    w.keepListed = rgba ? QStringList{".lum.pfm", ".black.pfm"} : QStringList{""};
    if(!writeCalibration(w, &err))
    {
      r.status = "Failed: " + err.toStdString();
      return r;
    }
  }
  else
  {
    const QString json = base + QStringLiteral(".json");
    const auto listed = readJsonFile(json).value("files").toArray();
    for(const auto& v : listed)
      if(QFileInfo{written}.fileName().compare(v.toString(), Qt::CaseInsensitive) == 0)
      {
        r.status = "Failed: " + json.toStdString()
                   + " lists this map; turn JSON sidecar on to update it";
        return r;
      }
    if(!score::gfx::writeFloatImageSplit(base, job.image, &err))
    {
      r.status = "Failed: " + err.toStdString();
      return r;
    }
  }
  r.ok = true;
  r.written = written.toStdString();
  r.status = "Saved " + r.written + " (" + std::to_string(job.image.width) + " x "
             + std::to_string(job.image.height) + ")";
  return r;
}

void SaveFloatTexture::worker::work(
    std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job)
{
  auto res = write(*job);
  if(auto j = jobs.lock())
    j->finish(id, std::move(res));
}

void SaveFloatTexture::operator()()
{
  if(auto r = m_jobs->fresh(m_seen))
  {
    outputs.saved.value = r->ok;
    outputs.written.value = r->written;
    outputs.status.value = r->status;
  }

  auto& in = inputs.image;
  if(inputs.save)
  {
    if(inputs.file.value.empty())
      outputs.status.value = "Failed: no file";
    else if(m_jobs->busy())
      outputs.status.value = "Failed: the previous save is not done";
    else
    {
      outputs.saved.value = false;
      outputs.status.value = "Reading the input";
      in.request_readback = true;
      m_waited = 0;
    }
  }
  if(m_waited < 0)
    return;

  const auto& t = in.texture;
  if(t.bytes && t.width > 0 && t.height > 0)
  {
    in.request_readback = false;
    m_waited = -1;
    if(in.resampled)
    {
      outputs.status.value = "Failed: In is not an RGBA32F texture it can read at its size";
      return;
    }
    Job job;
    job.path = QString::fromStdString(inputs.file.value);
    job.image = extract(t.bytes, t.width, t.height, inputs.channels.value);
    job.sidecar = inputs.sidecar.value;
    job.intrinsics = inputs.intrinsics.value;
    job.pose = inputs.pose.value;
    job.viewer = {inputs.viewer.value.x, inputs.viewer.value.y, inputs.viewer.value.z};
    job.rms = inputs.rms.value;
    outputs.status.value = "Saving";
    worker.request(m_jobs, m_jobs->start(), std::make_shared<const Job>(std::move(job)));
  }
  else if(++m_waited > readbackFrames)
  {
    in.request_readback = false;
    m_waited = -1;
    outputs.status.value = "Failed: nothing reached In";
  }
}
}
