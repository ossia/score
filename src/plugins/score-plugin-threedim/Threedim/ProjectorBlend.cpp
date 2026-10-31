#include "ProjectorBlend.hpp"

#include <Gfx/Calibration/Blending.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

#include <chrono>
#include <cmath>
#include <cstring>

namespace Threedim
{
namespace blend = score::gfx::blend;
namespace procam = score::gfx::procam;

namespace
{
float perProjector(const std::vector<float>& v, std::size_t i, float def)
{
  if(v.empty())
    return def;
  return i < v.size() ? v[i] : v.back();
}

bool writeMap(const QString& path, const std::vector<float>& v, int w, int h, int channels)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = channels;
  img.data = v;
  return score::gfx::writePFM(path, img);
}
}

ProjectorBlendMaps::Result
ProjectorBlendMaps::compute(std::vector<CalibrationFile> cals, const Settings& s)
{
  Result r;
  std::vector<blend::Projector> P;
  for(std::size_t i = 0; i < cals.size(); i++)
  {
    auto& c = cals[i];
    blend::Projector p;
    p.width = c.width;
    p.height = c.height;
    p.xyz = std::move(c.xyz);
    if(c.hasModel() && !s.invertMap)
    {
      const Eigen::Matrix4d W = procam::fromColumnMajor(c.pose);
      p.model.fx = c.intrinsics[0];
      p.model.fy = c.intrinsics[1];
      p.model.cx = c.intrinsics[2];
      p.model.cy = c.intrinsics[3];
      p.model.R = W.topLeftCorner<3, 3>().transpose();
      p.model.t = -p.model.R * W.topRightCorner<3, 1>();
      p.hasModel = true;
    }
    p.lum = std::move(c.lum);
    p.black = std::move(c.black);
    p.rampWidth = perProjector(s.ramp, i, 0.f);
    p.exponent = perProjector(s.exponent, i, 1.f);
    P.push_back(std::move(p));
  }
  if(P.empty())
  {
    r.status = "Failed: no calibration (set Calibration files or connect an XYZ map)";
    return r;
  }

  blend::Options opt;
  opt.visibilityTolerance = s.tolerance;
  opt.photometric = s.photometric;
  opt.blackLevel = s.blackLevel;
  opt.blackCompensation = s.blackCompensation;
  opt.luminanceUniformity = s.uniformity;
  opt.maxLogSlope = s.gradient;
  opt.hasViewer = s.useViewer;
  opt.viewer = blend::Vec3(s.viewer[0], s.viewer[1], s.viewer[2]);
  opt.perChannel = s.perChannel;

  const auto t0 = std::chrono::steady_clock::now();
  auto res = blend::compute(P, opt);
  const double secs
      = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if(res.status.starts_with("Failed"))
  {
    r.status = res.status;
    return r;
  }

  if(s.encodeGamma != 1.f && s.encodeGamma > 0.f)
    for(std::size_t i = 0; i < P.size(); i++)
    {
      for(float& a : res.alpha[i])
        a = std::pow(std::max(0.f, a), 1.f / s.encodeGamma);
      for(float& b : res.beta[i])
        b = std::pow(std::max(0.f, b), 1.f / s.encodeGamma);
    }

  r.gains = res.colorGain;
  r.overlap = res.overlap;
  int written = 0;
  for(std::size_t i = 0; i < P.size(); i++)
  {
    r.width.push_back(P[i].width);
    r.height.push_back(P[i].height);
    std::string af, bf;
    if(!cals[i].base.empty())
    {
      const QString qbase = QString::fromStdString(cals[i].base);
      if(writeMap(qbase + ".alpha.pfm", res.alpha[i], P[i].width, P[i].height, res.channels))
        af = QString(qbase + ".alpha.pfm").toStdString();
      if(writeMap(qbase + ".beta.pfm", res.beta[i], P[i].width, P[i].height, res.channels))
        bf = QString(qbase + ".beta.pfm").toStdString();
      QJsonObject o;
      o["alphaMap"] = QFileInfo(qbase + ".alpha.pfm").fileName();
      o["betaMap"] = QFileInfo(qbase + ".beta.pfm").fileName();
      o["linearLight"] = s.encodeGamma == 1.f;
      o["encodeGamma"] = s.encodeGamma;
      o["colorGainLinear"]
          = QJsonArray{res.colorGain[i][0], res.colorGain[i][1], res.colorGain[i][2]};
      o["overlap"] = res.overlap[i];
      o["channels"] = res.channels;
      if(res.channels == 3)
        o["whiteChroma"]
            = QJsonArray{res.whiteChroma[0], res.whiteChroma[1], res.whiteChroma[2]};
      o["projectorIndex"] = int(i);
      o["projectorCount"] = int(P.size());
      o["blackTarget"] = res.blackTarget;
      o["luminanceUniformity"] = s.uniformity;
      o["luminanceGradientPerRadian"] = s.gradient;
      o["rampWidth"] = P[i].rampWidth;
      o["exponent"] = P[i].exponent;
      o["reprojection"] = P[i].hasModel ? "pose" : "map";
      QFile f{qbase + ".blend.json"};
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson());
      written += !af.empty() && !bf.empty();
    }
    r.alphaFiles.push_back(af);
    r.betaFiles.push_back(bf);
  }
  r.alpha = std::move(res.alpha);
  r.beta = std::move(res.beta);
  r.channels = res.channels;
  r.ok = true;
  char buf[200];
  std::snprintf(
      buf, sizeof(buf), "Done: %d projectors in %.2f s, %d map pairs written, overlap",
      int(P.size()), secs, written);
  r.status = buf;
  if(res.channels == 3)
    r.status.insert(r.status.find(" map pairs"), " RGB");
  for(float o : r.overlap)
  {
    std::snprintf(buf, sizeof(buf), " %.0f%%", 100.f * o);
    r.status += buf;
  }
  if(res.blackTarget > 0.f)
  {
    std::snprintf(buf, sizeof(buf), ", black raised to %.4f", res.blackTarget);
    r.status += buf;
  }
  return r;
}

ProjectorBlendMaps::~ProjectorBlendMaps()
{
  if(m_future.valid())
    m_future.wait();
}

ProjectorBlendMaps::Settings ProjectorBlendMaps::settings() const
{
  Settings s;
  s.ramp = splitNumbers(inputs.ramp.value);
  s.exponent = splitNumbers(inputs.exponent.value);
  s.encodeGamma = inputs.encode_gamma.value;
  s.blackCompensation = inputs.black_compensation.value;
  s.blackLevel = inputs.black_level.value;
  s.photometric = inputs.photometric.value;
  s.uniformity = inputs.uniformity.value;
  s.gradient = inputs.gradient.value;
  s.useViewer = inputs.use_viewer.value;
  s.viewer = {inputs.viewer.value.x, inputs.viewer.value.y, inputs.viewer.value.z};
  s.tolerance = inputs.tolerance.value;
  s.invertMap = inputs.reprojection.value == InvertMap;
  s.perChannel = inputs.per_channel.value;
  return s;
}

void ProjectorBlendMaps::start()
{
  std::vector<CalibrationFile> textures;
  int k = 0;
  auto take = [&](auto& in) {
    k++;
    const auto& t = in.texture;
    in.request_readback = false;
    if(!t.bytes || t.width <= 0 || t.height <= 0)
      return;
    CalibrationFile c;
    c.width = t.width;
    c.height = t.height;
    c.xyz.assign(t.bytes, t.bytes + std::size_t(t.width) * t.height * 4);
    if(!inputs.prefix.value.empty())
      c.base = inputs.prefix.value + "_" + std::to_string(k);
    textures.push_back(std::move(c));
  };
  take(inputs.xyz1);
  take(inputs.xyz2);
  take(inputs.xyz3);
  take(inputs.xyz4);
  const auto paths = splitPaths(inputs.files.value);
  outputs.status.value = "Computing "
                         + std::to_string(paths.size() + textures.size()) + " projectors";
  m_future = std::async(
      std::launch::async,
      [paths, textures = std::move(textures), s = settings()]() mutable {
    std::vector<CalibrationFile> cals;
    std::string failed;
    for(const auto& path : paths)
    {
      auto c = loadCalibration(path);
      if(!c.ok())
      {
        failed += (failed.empty() ? "" : ", ") + c.error;
        continue;
      }
      cals.push_back(std::move(c));
    }
    for(auto& c : textures)
      cals.push_back(std::move(c));
    if(!failed.empty() && cals.empty())
    {
      Result r;
      r.status = "Failed: " + failed;
      return r;
    }
    return compute(std::move(cals), s);
  });
}

void ProjectorBlendMaps::publish(const Result& r)
{
  outputs.status.value = r.status;
  if(!r.ok)
    return;
  auto out = [&](auto& port, const std::vector<float>& v, int w, int h) {
    port.create(w, h);
    float* px = port.texture.bytes;
    const int C = r.channels;
    for(std::size_t i = 0; i < std::size_t(w) * h; i++)
    {
      for(int c = 0; c < 3; c++)
        px[i * 4 + c] = v[i * C + (C == 3 ? c : 0)];
      px[i * 4 + 3] = 1.f;
    }
    port.upload();
  };
  auto slot = [&](std::size_t i, auto& a, auto& b) {
    if(i < r.alpha.size())
    {
      out(a, r.alpha[i], r.width[i], r.height[i]);
      out(b, r.beta[i], r.width[i], r.height[i]);
    }
  };
  slot(0, outputs.alpha1, outputs.beta1);
  slot(1, outputs.alpha2, outputs.beta2);
  slot(2, outputs.alpha3, outputs.beta3);
  slot(3, outputs.alpha4, outputs.beta4);
  outputs.alpha_files.value = r.alphaFiles;
  outputs.beta_files.value = r.betaFiles;
  outputs.gains.value.clear();
  for(const auto& g : r.gains)
    outputs.gains.value.insert(outputs.gains.value.end(), g.begin(), g.end());
  outputs.overlap.value = r.overlap;
  outputs.done.value = halp::impulse{};
}

void ProjectorBlendMaps::operator()()
{
  outputs.done.value.reset();
  if(m_future.valid())
  {
    if(m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      publish(m_future.get());
    return;
  }
  if(m_waitTextures)
  {
    if(++m_waitFrames >= 3)
    {
      m_waitTextures = false;
      start();
    }
    return;
  }
  if(inputs.compute)
  {
    m_waitTextures = true;
    m_waitFrames = 0;
    inputs.xyz1.request_readback = inputs.xyz2.request_readback = true;
    inputs.xyz3.request_readback = inputs.xyz4.request_readback = true;
    outputs.status.value = "Reading the inputs";
  }
}
}
