#include "StructuredLightCalibration.hpp"

#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace Threedim
{
namespace
{
std::vector<float> parseExposures(const std::string& text)
{
  std::vector<float> out;
  for(const auto& t : QString::fromStdString(text).split(
          QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts))
    if(const float v = t.toFloat(); v > 0.f)
      out.push_back(v);
  return out;
}

float halfToFloat(uint16_t h) noexcept
{
  const uint32_t sign = uint32_t(h & 0x8000u) << 16;
  uint32_t exp = (h >> 10) & 0x1fu;
  uint32_t mant = h & 0x3ffu;
  uint32_t bits;
  if(exp == 0)
  {
    if(mant == 0)
      bits = sign;
    else
    {
      exp = 127 - 15 + 1;
      while(!(mant & 0x400u))
      {
        mant <<= 1;
        exp--;
      }
      bits = sign | (exp << 23) | ((mant & 0x3ffu) << 13);
    }
  }
  else if(exp == 31)
    bits = sign | 0x7f800000u | (mant << 13);
  else
    bits = sign | ((exp + 127 - 15) << 23) | (mant << 13);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
}

StructuredLightCalibration::~StructuredLightCalibration()
{
  if(node_state && node_state->driver == this)
    node_state->driver = nullptr;
}

StructuredLightCalibration::Shared& StructuredLightCalibration::shared()
{
  if(!node_state)
    node_state = std::make_shared<Shared>();
  return *node_state;
}

StructuredLightSession::Settings StructuredLightCalibration::settings() const
{
  StructuredLightSession::Settings s;
  s.projectorWidth = inputs.projector_size.value.x;
  s.projectorHeight = inputs.projector_size.value.y;
  s.projectors = std::clamp(inputs.projector_count.value, 1, 4);
  s.mode = inputs.mode.value;
  s.fov = inputs.fov.value;
  const auto& k = inputs.intrinsics.value;
  s.cameraIntrinsics = {k.x, k.y, k.z, k.w};
  s.worldFromCamera = inputs.world_from_camera.value;
  s.level = inputs.level.value;
  s.minStripe = inputs.min_stripe.value;
  s.settleMs = inputs.settle.value;
  s.timeoutMs = inputs.timeout.value;
  s.frames = inputs.frames.value;
  s.contrast = inputs.contrast.value;
  s.minSignal = inputs.min_signal.value;
  s.minBits = inputs.min_bits.value;
  s.holeRadius = inputs.hole_radius.value;
  s.depthJump = inputs.depth_jump.value;
  s.ransac = inputs.ransac.value;
  s.idle = inputs.idle.value;
  s.surface = inputs.surface.value;
  s.maxPlanes = inputs.max_planes.value;
  s.planeThreshold = inputs.plane_threshold.value;
  s.phaseShift = inputs.phase_shift.value;
  s.phaseSteps = inputs.phase_steps.value;
  s.phasePeriod = inputs.phase_period.value;
  s.projectorGamma = inputs.projector_gamma.value;
  s.phaseMinModulation = inputs.phase_min_modulation.value;
  s.intrinsicsMode = inputs.intrinsics_mode.value;
  const auto& lk = inputs.locked_intrinsics.value;
  s.lockedIntrinsics = {lk.x, lk.y, lk.z, lk.w};
  s.throwRatio = inputs.throw_ratio.value;
  s.lensShift = {inputs.lens_shift.value.x, inputs.lens_shift.value.y};
  s.accumulateIntrinsics = inputs.accumulate_intrinsics.value;
  s.cameraGamma = inputs.camera_gamma.value;
  s.responseLevels = inputs.response_levels.value;
  s.compensation = inputs.compensation.value;
  s.exposureSettleMs = inputs.exposure_settle.value;
  s.captureExposure = inputs.capture_exposure.value;
  s.compGamut = inputs.comp_gamut.value;
  s.compStrength = inputs.comp_strength.value;
  s.compGradient = inputs.comp_gradient.value;
  s.compBlack = inputs.comp_black.value;
  s.mixLevels = inputs.mix_levels.value;
  s.autoExposureAfter = inputs.auto_exposure_after.value;
  s.keepPixels = keepPixels;
  return s;
}

const float* StructuredLightCalibration::cameraFrame()
{
  const auto& t = inputs.camera.texture;
  if(!t.bytes || t.width <= 0 || t.height <= 0)
    return nullptr;
  const std::size_t n = std::size_t(t.width) * t.height;
  using F = halp::custom_variable_texture;
  switch(t.format)
  {
    case F::RGBA32F:
      return reinterpret_cast<const float*>(t.bytes);
    case F::RGBA8:
    case F::BGRA8:
    {
      m_frame.resize(n * 4);
      const bool bgr = t.format == F::BGRA8;
      for(std::size_t i = 0; i < n; i++)
      {
        const uint8_t* p = t.bytes + i * 4;
        m_frame[i * 4 + 0] = p[bgr ? 2 : 0] * (1.f / 255.f);
        m_frame[i * 4 + 1] = p[1] * (1.f / 255.f);
        m_frame[i * 4 + 2] = p[bgr ? 0 : 2] * (1.f / 255.f);
        m_frame[i * 4 + 3] = p[3] * (1.f / 255.f);
      }
      return m_frame.data();
    }
    case F::RGBA16F:
    {
      m_frame.resize(n * 4);
      const auto* h = reinterpret_cast<const uint16_t*>(t.bytes);
      for(std::size_t i = 0; i < n * 4; i++)
        m_frame[i] = halfToFloat(h[i]);
      return m_frame.data();
    }
    default:
      return nullptr;
  }
}

void StructuredLightCalibration::operator()()
{
  using namespace std::chrono;
  tick(duration<double, std::milli>(steady_clock::now().time_since_epoch()).count());
}

void StructuredLightCalibration::syncPorts(const StructuredLightSession& session)
{
  for(int p = 0; p < 4; p++)
  {
    const auto& d = session.image(p);
    const bool image = d.width > 0 && d.height > 0 && !d.rgba.empty();
    const int version = image ? d.version : -2;
    if(m_versions[p] == version)
      continue;
    m_versions[p] = version;
    auto write = [&](auto& port) {
      if(!image)
      {
        port.create(1, 1);
        std::memset(port.texture.bytes, 0, 3);
        port.texture.bytes[3] = 255;
      }
      else
      {
        port.create(d.width, d.height);
        std::memcpy(port.texture.bytes, d.rgba.data(), d.rgba.size());
      }
      port.upload();
    };
    switch(p)
    {
      case 1:
        write(outputs.pattern2);
        break;
      case 2:
        write(outputs.pattern3);
        break;
      case 3:
        write(outputs.pattern4);
        break;
      default:
        write(outputs.pattern);
        break;
    }
  }
  if(session.capturing())
    outputs.projector_index.value = session.projector();
}

void StructuredLightCalibration::writeEvents(const StructuredLightSession::Events& ev)
{
  if(ev.exposure)
    outputs.camera_exposure.value = ev.exposure;
  if(ev.autoExposure)
    outputs.auto_exposure.value = ev.autoExposure;
  if(ev.done)
    outputs.done.value = halp::impulse{};
}

void StructuredLightCalibration::publish(const Result& r)
{
  if(!r.ok)
    return;
  auto xyzOut = [&](auto& port) {
    port.create(r.width, r.height);
    std::memcpy(port.texture.bytes, r.xyz.data(), r.xyz.size() * sizeof(float));
    port.upload();
  };
  switch(r.projectorIndex)
  {
    case 0:
      xyzOut(outputs.xyz);
      break;
    case 1:
      xyzOut(outputs.xyz2);
      break;
    case 2:
      xyzOut(outputs.xyz3);
      break;
    case 3:
      xyzOut(outputs.xyz4);
      break;
    default:
      break;
  }
  if(r.projectorIndex > 0)
    return;
  auto rgba = [&](auto& port, const std::vector<float>& v) {
    if(v.size() != std::size_t(r.width) * r.height * 4)
      return;
    port.create(r.width, r.height);
    std::memcpy(port.texture.bytes, v.data(), v.size() * sizeof(float));
    port.upload();
  };
  rgba(outputs.lum, r.lum);
  rgba(outputs.black, r.black);
  if(!r.decoded.empty())
  {
    outputs.decoded.create(r.camWidth, r.camHeight);
    std::memcpy(outputs.decoded.texture.bytes, r.decoded.data(), r.decoded.size());
    outputs.decoded.upload();
  }
  if(r.intrinsics[0] > 0.f)
  {
    outputs.intrinsics.value = r.intrinsics;
    outputs.pose.value = r.pose;
    for(int c = 0; c < 4; c++)
    {
      auto& col = c == 0   ? outputs.m0.value
                  : c == 1 ? outputs.m1.value
                  : c == 2 ? outputs.m2.value
                           : outputs.m3.value;
      for(int k = 0; k < 4; k++)
        col[k] = r.pose[c * 4 + k];
    }
  }
  outputs.rms.value = r.rms;
  outputs.coverage.value = r.coverage;
  outputs.planes.value = r.planes;
}

void StructuredLightCalibration::drive(Shared& sh, double now)
{
  auto& session = sh.session;
  const auto s = settings();
  StructuredLightSession::Events ev;
  if(m_loadWanted)
  {
    m_loadWanted = false;
    if(!inputs.load.value.empty() && !session.running())
      session.load(QString::fromStdString(inputs.load.value));
  }
  if(inputs.save)
  {
    if(inputs.save_path.value.empty())
      session.setStatus("Failed: no Save file");
    else
      session.save(QString::fromStdString(inputs.save_path.value));
  }
  if(inputs.abort && session.running())
    session.abort(s, ev);
  if(inputs.start && !session.running())
  {
    StructuredLightSession::Run run;
    run.exposures = parseExposures(inputs.exposures.value);
    run.cameraResponse = QString::fromStdString(inputs.crf_file.value);
    if(inputs.auto_save.value)
      run.autoSave = QString::fromStdString(inputs.save_path.value);
    session.start(s, run, now, ev);
  }

  StructuredLightSession::Frame frame;
  if(session.wantsCamera())
  {
    if(inputs.camera.resampled && inputs.camera.texture.bytes)
      session.abort(
          s, ev,
          "Failed: the camera reached the process resampled (multisampled, or not "
          "readable at its size)");
    else if(const float* rgba = cameraFrame())
      frame.camera = {rgba, inputs.camera.texture.width, inputs.camera.texture.height};
  }
  if(const auto& t = inputs.positions.texture;
     session.wantsPositions() && t.bytes && t.width > 0 && t.height > 0)
  {
    if(inputs.positions.resampled)
      session.abort(
          s, ev,
          "Failed: the positions reached the process resampled: give them an RGBA32F "
          "texture it can read at its size");
    else
    {
      frame.positions = t.bytes;
      frame.positionsWidth = t.width;
      frame.positionsHeight = t.height;
    }
  }
  session.tick(s, now, frame, ev);
  inputs.camera.request_readback = session.wantsCamera();
  inputs.positions.request_readback = session.wantsPositions();
  writeEvents(ev);
}

void StructuredLightCalibration::tick(double now)
{
  outputs.done.value.reset();
  outputs.camera_exposure.value.reset();
  outputs.auto_exposure.value.reset();

  auto& sh = shared();
  if(drives_node || !sh.driver)
  {
    if(sh.driver != this)
    {
      sh.driver = this;
      sh.session.setExecutor([request = worker.request](std::function<void()> job) {
        if(request)
          request(std::move(job));
        else
          job();
      });
    }
  }
  if(sh.driver == this)
    drive(sh, now);
  else
  {
    inputs.camera.request_readback = false;
    inputs.positions.request_readback = false;
  }

  auto& session = sh.session;
  if(m_published != session.resultVersion())
  {
    m_published = session.resultVersion();
    for(const auto& r : session.results())
      publish(*r);
  }
  if(outputs.status.value != session.status())
    outputs.status.value = session.status();
  outputs.progress.value = session.progress();
  outputs.measured_gamma.value = session.measuredGamma();
  if(inputs.accumulate_intrinsics.value)
    outputs.accumulated_intrinsics.value = session.accumulatedIntrinsics();
  syncPorts(session);
}
}
