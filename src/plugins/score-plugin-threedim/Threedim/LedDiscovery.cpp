#include "LedDiscovery.hpp"

#include <Gfx/Calibration/LedMapping.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>

#include <QFile>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Threedim
{
namespace led = score::gfx::led;

void LedDiscovery::show(int frame)
{
  const int n = std::max(1, inputs.count.value);
  const auto code = led::BlinkCode::make(n);
  const float level = std::clamp(inputs.level.value, 0.f, 1.f);
  auto& vals = m_values;
  auto& cols = m_colours;
  vals.resize(n);
  cols.resize(std::size_t(n) * 3);
  const int pw = std::min(n, patternWidth);
  const int ph = (n + pw - 1) / pw;
  outputs.pattern.create(pw, ph);
  std::fill_n(outputs.pattern.texture.bytes, std::size_t(pw) * ph * 4, uint8_t(0));
  for(int i = 0; i < n; i++)
  {
    const bool on = frame >= 0 && code.on(frame, i);
    vals[i] = on ? level : 0.f;
    const float c = on ? std::round(level * 255.f) : 0.f;
    cols[i * 3] = cols[i * 3 + 1] = cols[i * 3 + 2] = c;
    uint8_t* px = outputs.pattern.texture.bytes + std::size_t(i) * 4;
    px[0] = px[1] = px[2] = uint8_t(c);
    px[3] = 255;
  }
  outputs.pattern.upload();
  outputs.values.value = vals;
  outputs.colours.value = cols;
}

void LedDiscovery::operator()()
{
  using namespace std::chrono;
  tick(duration<double, std::milli>(steady_clock::now().time_since_epoch()).count());
}

void LedDiscovery::stop(const std::string& status)
{
  m_frame = -1;
  m_captures.clear();
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  outputs.status.value = status;
  show(1);
}

void LedDiscovery::finish()
{
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  const int n = std::max(1, inputs.count.value);
  const auto code = led::BlinkCode::make(n);
  led::DecodeOptions opt;
  opt.minSignal = inputs.min_signal.value;
  opt.minContrast = inputs.min_contrast.value;
  opt.minPixels = inputs.min_pixels.value;

  std::vector<float> world;
  const std::vector<float>* pos = nullptr;
  if(m_positionFrames > 0 && m_positions.size() == std::size_t(m_camW) * m_camH * 4)
  {
    const Eigen::Matrix4d M
        = score::gfx::procam::fromColumnMajor(inputs.world_from_camera.value);
    world.resize(m_positions.size());
    for(std::size_t i = 0; i < m_positions.size(); i += 4)
    {
      const float w = m_positions[i + 3];
      if(w <= 0.f)
        continue;
      const Eigen::Vector4d p = M
                                * Eigen::Vector4d(
                                    m_positions[i] / w, m_positions[i + 1] / w,
                                    m_positions[i + 2] / w, 1.);
      world[i] = float(p.x());
      world[i + 1] = float(p.y());
      world[i + 2] = float(p.z());
      world[i + 3] = 1.f;
    }
    pos = &world;
  }
  const auto found = led::decode(code, m_captures, m_camW, m_camH, opt, pos);
  m_leds.assign(n, {});
  for(int i = 0; i < n; i++)
    m_leds[i].index = i;
  for(const auto& f : found)
  {
    auto& l = m_leds[f.index];
    l.found = true;
    l.u = float(f.u);
    l.v = float(f.v);
    l.nu = float(f.nu);
    l.nv = float(f.nv);
    l.pixels = f.pixels;
    l.confidence = float(f.confidence);
    if(f.hasWorld)
    {
      l.hasWorld = true;
      l.x = float(f.world.x());
      l.y = float(f.world.y());
      l.z = float(f.world.z());
    }
  }
  std::vector<float> uv(std::size_t(n) * 4, 0.f), worldOut(std::size_t(n) * 4, 0.f);
  int nWorld = 0;
  for(int i = 0; i < n; i++)
  {
    const auto& l = m_leds[i];
    if(!l.found)
      continue;
    float* q = uv.data() + std::size_t(i) * 4;
    q[0] = l.nu;
    q[1] = l.nv;
    q[3] = 1.f;
    if(l.hasWorld)
    {
      float* w = worldOut.data() + std::size_t(i) * 4;
      w[0] = l.x;
      w[1] = l.y;
      w[2] = l.z;
      w[3] = 1.f;
      nWorld++;
    }
  }
  outputs.uv.value = std::move(uv);
  outputs.world.value = std::move(worldOut);
  outputs.found.value = int(found.size());
  char buf[200];
  std::snprintf(
      buf, sizeof(buf), "Done: %d / %d LEDs found (%d with a world point), %d frames",
      int(found.size()), n, nWorld, code.frames());
  outputs.status.value = buf;
  outputs.progress.value = 1.f;
  outputs.done.value = halp::impulse{};
  m_frame = -1;
  m_captures.clear();
  show(1);
}

void LedDiscovery::tick(double now)
{
  outputs.done.value.reset();
  outputs.values.value.reset();
  outputs.colours.value.reset();
  outputs.uv.value.reset();
  outputs.world.value.reset();
  if(inputs.save && !m_leds.empty())
    outputs.status.value = writeCsv(inputs.csv.value, m_leds)
                               ? "Saved " + inputs.csv.value
                               : "Failed: cannot write " + inputs.csv.value;
  if(inputs.abort && running())
    stop("Aborted");
  if(inputs.start && !running())
  {
    m_frame = 0;
    m_captures.clear();
    m_camW = m_camH = 0;
    m_positions.clear();
    m_positionFrames = 0;
    m_settled = false;
    m_taken = 0;
    m_sum.clear();
    m_shownAt = now;
    inputs.camera.request_readback = true;
    inputs.positions.request_readback = true;
    show(0);
    outputs.progress.value = 0.f;
    outputs.status.value = "Frame 1 / "
                           + std::to_string(led::BlinkCode::make(std::max(1, inputs.count.value)).frames());
  }
  if(!running())
  {
    if(m_values.size() != std::size_t(std::max(1, inputs.count.value)))
      show(1);
    return;
  }

  const auto code = led::BlinkCode::make(std::max(1, inputs.count.value));
  if(!m_settled)
  {
    if(now - m_shownAt < inputs.settle.value)
      return;
    m_settled = true;
    m_taken = 0;
    m_sum.clear();
  }
  if(now - m_lastAt < 30.)
    return;
  const auto& t = inputs.camera.texture;
  if(!t.bytes || t.width <= 0 || t.height <= 0)
    return;
  if(m_camW == 0)
  {
    m_camW = t.width;
    m_camH = t.height;
  }
  if(t.width != m_camW || t.height != m_camH)
  {
    stop("Aborted: the camera size changed");
    return;
  }
  m_lastAt = now;
  const std::size_t n = std::size_t(m_camW) * m_camH;
  if(m_sum.size() != n)
    m_sum.assign(n, 0.f);
  for(std::size_t i = 0; i < n; i++)
    m_sum[i] += (t.bytes[i * 4] + t.bytes[i * 4 + 1] + t.bytes[i * 4 + 2]) * (1.f / 3.f);

  const auto& pt = inputs.positions.texture;
  if(m_frame == 0 && pt.bytes && pt.width > 0 && pt.height > 0)
  {
    if(m_positions.size() != n * 4)
      m_positions.assign(n * 4, 0.f);
    m_positionFrames++;
    for(int y = 0; y < m_camH; y++)
    {
      const int py = std::min(pt.height - 1, int((y + 0.5) * pt.height / m_camH));
      for(int x = 0; x < m_camW; x++)
      {
        const int px = std::min(pt.width - 1, int((x + 0.5) * pt.width / m_camW));
        const float* p = pt.bytes + (std::size_t(py) * pt.width + px) * 4;
        if(!(p[3] > 0.f) || !(p[2] > 0.f) || !std::isfinite(p[0]) || !std::isfinite(p[1]))
          continue;
        float* q = m_positions.data() + (std::size_t(y) * m_camW + x) * 4;
        q[0] += p[0];
        q[1] += p[1];
        q[2] += p[2];
        q[3] += 1.f;
      }
    }
  }

  if(++m_taken < std::max(1, inputs.frames.value))
    return;
  for(float& v : m_sum)
    v /= float(m_taken);
  m_captures.push_back(std::move(m_sum));
  m_sum.clear();
  m_frame++;
  outputs.progress.value = float(m_frame) / float(code.frames());
  if(m_frame >= code.frames())
  {
    finish();
    return;
  }
  inputs.positions.request_readback = false;
  outputs.status.value
      = "Frame " + std::to_string(m_frame + 1) + " / " + std::to_string(code.frames());
  show(m_frame);
  m_shownAt = now;
  m_settled = false;
}

bool LedDiscovery::writeCsv(const std::string& path, const std::vector<Led>& leds)
{
  QFile f{QString::fromStdString(path)};
  if(path.empty() || !f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return false;
  f.write("index,found,u,v,u_norm,v_norm,x,y,z,has_world,pixels,confidence\n");
  char buf[256];
  for(const auto& l : leds)
  {
    std::snprintf(
        buf, sizeof(buf), "%d,%d,%.3f,%.3f,%.6f,%.6f,%.5f,%.5f,%.5f,%d,%d,%.3f\n", l.index,
        int(l.found), l.u, l.v, l.nu, l.nv, l.x, l.y, l.z, int(l.hasWorld), l.pixels,
        l.confidence);
    f.write(buf);
  }
  return true;
}
}
