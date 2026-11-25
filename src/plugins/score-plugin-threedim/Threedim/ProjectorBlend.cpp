#include "ProjectorBlend.hpp"

#include <Gfx/Calibration/Blending.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Window/CalibrationSet.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

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
  for(std::size_t i = 0; i < P.size(); i++)
  {
    r.width.push_back(P[i].width);
    r.height.push_back(P[i].height);
  }
  r.alpha = std::move(res.alpha);
  r.beta = std::move(res.beta);
  r.channels = res.channels;
  r.whiteChroma = res.whiteChroma;
  r.blackTarget = res.blackTarget;
  for(const auto& p : P)
    r.reprojected.push_back(p.hasModel);
  for(const auto& p : P)
  {
    r.rampWidth.push_back(p.rampWidth);
    r.exponent.push_back(p.exponent);
  }
  r.seconds = secs;
  r.ok = true;
  return r;
}

void ProjectorBlendMaps::write(
    Result& r, const std::vector<QString>& bases, const Settings& s)
{
  if(!r.ok)
    return;
  const std::size_t n = r.alpha.size();
  int written = 0;
  std::string failed;
  r.alphaFiles.assign(n, {});
  r.betaFiles.assign(n, {});
  auto image = [&](const std::vector<float>& v, std::size_t i) {
    score::gfx::FloatImage img;
    img.width = r.width[i];
    img.height = r.height[i];
    img.channels = r.channels;
    img.data = v;
    return img;
  };
  auto writeBlend = [&](const QString& base, std::size_t i, QString& err) {
    const auto json = readJsonFile(base + ".json");
    const int jw = json.value("width").toInt(), jh = json.value("height").toInt();
    if(jw > 0 && jh > 0 && (jw != r.width[i] || jh != r.height[i]))
    {
      err = QStringLiteral("%1.json is a %2 x %3 projector, the map is %4 x %5")
                .arg(base)
                .arg(jw)
                .arg(jh)
                .arg(r.width[i])
                .arg(r.height[i]);
      return false;
    }
    if(!writeCalibrationMap(base, {".alpha.pfm", image(r.alpha[i], i)}, &err))
      return false;
    if(s.blackCompensation)
    {
      if(!writeCalibrationMap(base, {".beta.pfm", image(r.beta[i], i)}, &err))
        return false;
    }
    else
      QFile::remove(base + ".beta.pfm");
    QJsonObject o;
    o["alphaMap"] = QFileInfo(base + ".alpha.pfm").fileName();
    if(s.blackCompensation)
      o["betaMap"] = QFileInfo(base + ".beta.pfm").fileName();
    o["linearLight"] = s.encodeGamma == 1.f;
    o["encodeGamma"] = s.encodeGamma;
    o["colorGainLinear"] = QJsonArray{r.gains[i][0], r.gains[i][1], r.gains[i][2]};
    o["overlap"] = r.overlap[i];
    o["channels"] = r.channels;
    if(r.channels == 3)
      o["whiteChroma"] = QJsonArray{r.whiteChroma[0], r.whiteChroma[1], r.whiteChroma[2]};
    o["projectorIndex"] = int(i);
    o["projectorCount"] = int(n);
    if(s.blackCompensation)
      o["blackTarget"] = r.blackTarget;
    o["luminanceUniformity"] = s.uniformity;
    o["luminanceGradientPerRadian"] = s.gradient;
    o["rampWidth"] = r.rampWidth[i];
    o["exponent"] = r.exponent[i];
    o["reprojection"] = r.reprojected[i] ? "pose" : "map";
    return writeJsonFile(base + ".blend.json", o, &err);
  };
  for(std::size_t i = 0; i < n && i < bases.size(); i++)
  {
    const QString& base = bases[i];
    if(base.isEmpty())
      continue;
    QString err;
    if(writeBlend(base, i, err))
    {
      r.alphaFiles[i] = QString(base + ".alpha.pfm").toStdString();
      if(s.blackCompensation)
        r.betaFiles[i] = QString(base + ".beta.pfm").toStdString();
      written++;
      continue;
    }
    failed += (failed.empty() ? "" : "; ") + err.toStdString();
  }

  char buf[200];
  std::snprintf(
      buf, sizeof(buf), "Done: %d projectors in %.2f s, %d %s written, overlap",
      int(n), r.seconds, written,
      s.blackCompensation ? (r.channels == 3 ? "RGB map pairs" : "map pairs")
                          : (r.channels == 3 ? "RGB alpha maps" : "alpha maps"));
  r.status = buf;
  for(float o : r.overlap)
  {
    std::snprintf(buf, sizeof(buf), " %.0f%%", 100.f * o);
    r.status += buf;
  }
  if(s.blackCompensation && r.blackTarget > 0.f)
  {
    std::snprintf(buf, sizeof(buf), ", black raised to %.4f", r.blackTarget);
    r.status += buf;
  }
  if(!failed.empty())
    r.status += "; not written: " + failed;
}

ProjectorBlendMaps::Result ProjectorBlendMaps::run(const Job& job)
{
  std::vector<CalibrationFile> cals;
  std::string failed;
  int screens = 0;
  for(const auto& path : job.paths)
  {
    auto c = loadCalibration(QString::fromStdString(path));
    if(!c.ok())
    {
      failed += (failed.empty() ? "" : ", ") + c.error.toStdString();
      continue;
    }
    if(c.screen)
    {
      screens++;
      continue;
    }
    cals.push_back(std::move(c));
  }
  if(!failed.empty())
  {
    Result r;
    r.status = "Failed: " + failed;
    return r;
  }
  for(const auto& c : job.textures)
    cals.push_back(c);
  std::vector<QString> bases;
  for(const auto& c : cals)
    bases.push_back(c.base);
  auto r = compute(std::move(cals), job.settings);
  write(r, bases, job.settings);
  if(screens > 0)
    r.status += "; " + std::to_string(screens) + " screen(s) left out: nothing to blend";
  return r;
}

void ProjectorBlendMaps::worker::work(
    std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job)
{
  auto r = run(*job);
  if(auto j = jobs.lock())
    j->finish(id, std::move(r));
}

ProjectorBlendMaps::~ProjectorBlendMaps()
{
  if(node_state)
    node_state->release(this);
}

ProjectorBlendMaps::Jobs& ProjectorBlendMaps::jobs()
{
  if(!node_state)
    node_state = std::make_shared<Jobs>();
  return *node_state;
}

std::string ProjectorBlendMaps::parseSettings(const ins& in, Settings& s)
{
  const auto ramp = parseNumbers(in.ramp.value);
  const auto exponent = parseNumbers(in.exponent.value);
  if(!ramp)
    return "Ramp widths are not numbers: " + in.ramp.value;
  if(!exponent)
    return "Blend exponents are not numbers: " + in.exponent.value;
  for(float v : *ramp)
    if(v < 0.f)
      return "Ramp widths are not positive: " + in.ramp.value;
  for(float v : *exponent)
    if(v <= 0.f)
      return "Blend exponents are not positive: " + in.exponent.value;
  s.ramp = *ramp;
  s.exponent = *exponent;
  s.encodeGamma = in.encode_gamma.value;
  s.blackCompensation = in.black_compensation.value;
  s.blackLevel = in.black_level.value;
  s.photometric = in.photometric.value;
  s.uniformity = in.uniformity.value;
  s.gradient = in.gradient.value;
  s.useViewer = in.use_viewer.value;
  s.viewer = {in.viewer.value.x, in.viewer.value.y, in.viewer.value.z};
  s.tolerance = in.tolerance.value;
  s.invertMap = in.reprojection.value == InvertMap;
  s.perChannel = in.per_channel.value;
  return {};
}

void ProjectorBlendMaps::start()
{
  auto job = std::make_shared<Job>();
  if(const auto err = parseSettings(inputs, job->settings); !err.empty())
  {
    outputs.status.value = "Failed: " + err;
    return;
  }
  int k = 0;
  std::string refused;
  auto take = [&](auto& in) {
    const int index = k++;
    const auto& t = in.texture;
    in.request_readback = false;
    if(!t.bytes || t.width <= 0 || t.height <= 0)
      return;
    if(in.resampled)
    {
      refused += (refused.empty() ? "" : ", ") + std::to_string(index + 1);
      return;
    }
    CalibrationFile c;
    c.width = t.width;
    c.height = t.height;
    c.xyz.assign(t.bytes, t.bytes + std::size_t(t.width) * t.height * 4);
    if(!inputs.prefix.value.empty())
      c.base = Gfx::calibrationSetName(
          Gfx::calibrationBase(QString::fromStdString(inputs.prefix.value)), index);
    job->textures.push_back(std::move(c));
  };
  take(inputs.xyz1);
  take(inputs.xyz2);
  take(inputs.xyz3);
  take(inputs.xyz4);
  if(!refused.empty())
  {
    outputs.status.value = "Failed: XYZ map " + refused
                           + " is not an RGBA32F texture it can read at its size";
    return;
  }
  job->paths = splitPaths(inputs.files.value);
  if(job->paths.empty() && job->textures.empty())
  {
    outputs.status.value
        = "Failed: no calibration (set Calibration files or connect an XYZ map)";
    return;
  }
  outputs.status.value = "Computing "
                         + std::to_string(job->paths.size() + job->textures.size())
                         + " projectors";
  worker.request(node_state, jobs().start(), std::move(job));
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
  auto& j = jobs();
  if(auto r = j.fresh(m_seen))
    publish(*r);

  if(!j.claim(this))
    return;
  if(m_waitFrames >= 0)
  {
    if(++m_waitFrames >= readbackFrames)
    {
      m_waitFrames = -1;
      start();
    }
    return;
  }
  if(inputs.compute)
  {
    if(j.busy())
    {
      outputs.status.value = "Failed: the previous computation is not done";
      return;
    }
    m_waitFrames = 0;
    inputs.xyz1.request_readback = inputs.xyz2.request_readback = true;
    inputs.xyz3.request_readback = inputs.xyz4.request_readback = true;
    outputs.status.value = "Reading the inputs";
  }
}
}
