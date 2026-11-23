#include "LedDiscovery.hpp"

#include <Gfx/Calibration/LedMapping.hpp>

#include <QSaveFile>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Threedim
{
namespace led = score::gfx::led;

LedDiscovery::~LedDiscovery()
{
  if(node_state)
    node_state->decodes.release(this);
}

LedDiscovery::Shared& LedDiscovery::state()
{
  if(!node_state)
    node_state = std::make_shared<Shared>();
  return *node_state;
}

void LedDiscovery::setStatus(std::string s)
{
  auto& st = state();
  st.status = std::move(s);
  st.statuses++;
}

void LedDiscovery::show(int frame)
{
  auto& st = state();
  const int n = std::max(1, inputs.count.value);
  const auto code = led::BlinkCode::make(n);
  const float level = std::clamp(inputs.level.value, 0.f, 1.f);
  st.values.resize(n);
  st.colours.resize(std::size_t(n) * 3);
  st.patternW = std::min(n, patternWidth);
  st.patternH = (n + st.patternW - 1) / st.patternW;
  st.pattern.assign(std::size_t(st.patternW) * st.patternH * 4, uint8_t(0));
  for(int i = 0; i < n; i++)
  {
    const bool on = frame >= 0 && code.on(frame, i);
    st.values[i] = on ? level : 0.f;
    const float c = on ? std::round(level * 255.f) : 0.f;
    st.colours[i * 3] = st.colours[i * 3 + 1] = st.colours[i * 3 + 2] = c;
    uint8_t* px = st.pattern.data() + std::size_t(i) * 4;
    px[0] = px[1] = px[2] = uint8_t(c);
    px[3] = 255;
  }
  st.shown++;
}

void LedDiscovery::operator()()
{
  using namespace std::chrono;
  tick(duration<double, std::milli>(steady_clock::now().time_since_epoch()).count());
}

void LedDiscovery::stop(const std::string& status)
{
  auto& st = state();
  st.frame = -1;
  st.captures.clear();
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  setStatus(status);
  show(1);
}

LedDiscovery::Decoded LedDiscovery::decode(const DecodeJob& job)
{
  Decoded out;
  const int n = std::max(1, job.count);
  const auto code = led::BlinkCode::make(n);
  led::DecodeOptions opt;
  opt.minSignal = job.minSignal;
  opt.minContrast = job.minContrast;
  opt.minPixels = job.minPixels;

  std::vector<float> world;
  const std::vector<float>* pos = nullptr;
  if(!job.positions.empty()
     && job.positions.size() == std::size_t(job.width) * job.height * 4)
  {
    const Eigen::Matrix4d M = score::gfx::procam::fromColumnMajor(job.worldFromCamera);
    world.resize(job.positions.size());
    for(std::size_t i = 0; i < job.positions.size(); i += 4)
    {
      const float w = job.positions[i + 3];
      if(w <= 0.f)
        continue;
      const Eigen::Vector4d p
          = M
            * Eigen::Vector4d(
                job.positions[i] / w, job.positions[i + 1] / w, job.positions[i + 2] / w,
                1.);
      world[i] = float(p.x());
      world[i + 1] = float(p.y());
      world[i + 2] = float(p.z());
      world[i + 3] = 1.f;
    }
    pos = &world;
  }
  const auto found = led::decode(code, job.captures, job.width, job.height, opt, pos);
  out.leds.assign(n, {});
  for(int i = 0; i < n; i++)
    out.leds[i].index = i;
  for(const auto& f : found)
  {
    auto& l = out.leds[f.index];
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
  out.uv.assign(std::size_t(n) * 4, 0.f);
  out.world.assign(std::size_t(n) * 4, 0.f);
  int nWorld = 0;
  for(int i = 0; i < n; i++)
  {
    const auto& l = out.leds[i];
    if(!l.found)
      continue;
    float* q = out.uv.data() + std::size_t(i) * 4;
    q[0] = l.nu;
    q[1] = l.nv;
    q[3] = 1.f;
    if(l.hasWorld)
    {
      float* w = out.world.data() + std::size_t(i) * 4;
      w[0] = l.x;
      w[1] = l.y;
      w[2] = l.z;
      w[3] = 1.f;
      nWorld++;
    }
  }
  out.found = int(found.size());
  char buf[200];
  std::snprintf(
      buf, sizeof(buf), "Done: %d / %d LEDs found (%d with a world point), %d frames",
      int(found.size()), n, nWorld, code.frames());
  out.status = buf;
  return out;
}

void LedDiscovery::worker::work(
    std::weak_ptr<Shared> shared, uint64_t id, std::shared_ptr<const Job> job)
{
  std::visit(
      [&]<typename J>(const J& j) {
    if constexpr(std::is_same_v<J, DecodeJob>)
    {
      auto r = decode(j);
      if(auto s = shared.lock())
        s->decodes.finish(id, std::move(r));
    }
    else
    {
      Written r;
      r.status
          = writeCsv(j.path, j.leds) ? "Saved " + j.path : "Failed: cannot write " + j.path;
      if(auto s = shared.lock())
        s->writes.finish(id, std::move(r));
    }
  },
      job->job);
}

void LedDiscovery::finish()
{
  auto& st = state();
  inputs.camera.request_readback = false;
  inputs.positions.request_readback = false;
  DecodeJob job;
  job.count = std::max(1, inputs.count.value);
  job.captures = std::move(st.captures);
  job.width = st.camW;
  job.height = st.camH;
  if(st.positionFrames > 0)
    job.positions = std::move(st.positions);
  job.worldFromCamera = inputs.world_from_camera.value;
  job.minSignal = inputs.min_signal.value;
  job.minContrast = inputs.min_contrast.value;
  job.minPixels = inputs.min_pixels.value;
  st.captures.clear();
  st.positions.clear();
  st.frame = -1;
  st.decoding = true;
  setStatus("Decoding");
  worker.request(
      node_state, st.decodes.start(), std::make_shared<const Job>(Job{std::move(job)}));
  show(1);
}

void LedDiscovery::drive(double now)
{
  auto& st = state();
  if(auto r = st.decodes.fresh(m_decodedSeen))
  {
    st.decoding = false;
    st.leds = r->leds;
    st.result = *r;
    st.results++;
    st.progress = 1.f;
    setStatus(r->status);
  }
  if(auto w = st.writes.fresh(m_writtenSeen))
    setStatus(w->status);

  if(inputs.save)
  {
    if(st.leds.empty())
      setStatus("Failed: no discovery to save");
    else if(inputs.csv.value.empty())
      setStatus("Failed: no CSV file");
    else
      worker.request(
          node_state, st.writes.start(),
          std::make_shared<const Job>(Job{CsvJob{inputs.csv.value, st.leds}}));
  }
  if(inputs.abort && st.frame >= 0)
    stop("Aborted");
  if(inputs.start && !running())
  {
    st.frame = 0;
    st.captures.clear();
    st.camW = st.camH = 0;
    st.positions.clear();
    st.positionFrames = 0;
    st.settled = false;
    st.taken = 0;
    st.sum.clear();
    st.shownAt = now;
    inputs.camera.request_readback = true;
    inputs.positions.request_readback = true;
    show(0);
    st.progress = 0.f;
    setStatus(
        "Frame 1 / "
        + std::to_string(led::BlinkCode::make(std::max(1, inputs.count.value)).frames()));
  }
  if(st.frame < 0)
  {
    if(st.values.size() != std::size_t(std::max(1, inputs.count.value)))
      show(1);
    return;
  }

  const auto code = led::BlinkCode::make(std::max(1, inputs.count.value));
  if(!st.settled)
  {
    if(now - st.shownAt < inputs.settle.value)
      return;
    st.settled = true;
    st.taken = 0;
    st.sum.clear();
  }
  if(now - st.lastAt < 30.)
    return;
  const auto& t = inputs.camera.texture;
  if(!t.bytes || t.width <= 0 || t.height <= 0)
    return;
  if(st.camW == 0)
  {
    st.camW = t.width;
    st.camH = t.height;
  }
  if(t.width != st.camW || t.height != st.camH)
  {
    stop("Aborted: the camera size changed");
    return;
  }
  st.lastAt = now;
  const std::size_t n = std::size_t(st.camW) * st.camH;
  if(st.sum.size() != n)
    st.sum.assign(n, 0.f);
  for(std::size_t i = 0; i < n; i++)
    st.sum[i] += (t.bytes[i * 4] + t.bytes[i * 4 + 1] + t.bytes[i * 4 + 2]) * (1.f / 3.f);

  const auto& pt = inputs.positions.texture;
  if(st.frame == 0 && pt.bytes && pt.width > 0 && pt.height > 0)
  {
    if(st.positions.size() != n * 4)
      st.positions.assign(n * 4, 0.f);
    st.positionFrames++;
    for(int y = 0; y < st.camH; y++)
    {
      const int py = std::min(pt.height - 1, int((y + 0.5) * pt.height / st.camH));
      for(int x = 0; x < st.camW; x++)
      {
        const int px = std::min(pt.width - 1, int((x + 0.5) * pt.width / st.camW));
        const float* p = pt.bytes + (std::size_t(py) * pt.width + px) * 4;
        if(!(p[3] > 0.f) || !(p[2] > 0.f) || !std::isfinite(p[0]) || !std::isfinite(p[1]))
          continue;
        float* q = st.positions.data() + (std::size_t(y) * st.camW + x) * 4;
        q[0] += p[0];
        q[1] += p[1];
        q[2] += p[2];
        q[3] += 1.f;
      }
    }
  }

  if(++st.taken < std::max(1, inputs.frames.value))
    return;
  for(float& v : st.sum)
    v /= float(st.taken);
  st.captures.push_back(std::move(st.sum));
  st.sum.clear();
  st.frame++;
  st.progress = float(st.frame) / float(code.frames());
  if(st.frame >= code.frames())
  {
    finish();
    return;
  }
  inputs.positions.request_readback = false;
  setStatus("Frame " + std::to_string(st.frame + 1) + " / " + std::to_string(code.frames()));
  show(st.frame);
  st.shownAt = now;
  st.settled = false;
}

void LedDiscovery::mirror()
{
  auto& st = state();
  if(m_shownSeen != st.shown)
  {
    m_shownSeen = st.shown;
    outputs.values.value = st.values;
    outputs.colours.value = st.colours;
    outputs.pattern.create(st.patternW, st.patternH);
    std::copy(st.pattern.begin(), st.pattern.end(), outputs.pattern.texture.bytes);
    outputs.pattern.upload();
  }
  if(m_resultSeen != st.results)
  {
    m_resultSeen = st.results;
    outputs.uv.value = st.result.uv;
    outputs.world.value = st.result.world;
    outputs.found.value = st.result.found;
    outputs.done.value = halp::impulse{};
  }
  if(m_statusSeen != st.statuses)
  {
    m_statusSeen = st.statuses;
    outputs.status.value = st.status;
  }
  outputs.progress.value = st.progress;
}

void LedDiscovery::tick(double now)
{
  outputs.done.value.reset();
  outputs.values.value.reset();
  outputs.colours.value.reset();
  outputs.uv.value.reset();
  outputs.world.value.reset();
  if(state().decodes.claim(this))
    drive(now);
  mirror();
}

bool LedDiscovery::writeCsv(const std::string& path, const std::vector<Led>& leds)
{
  QSaveFile f{QString::fromStdString(path)};
  if(path.empty() || !f.open(QIODevice::WriteOnly | QIODevice::Text))
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
  return f.commit();
}
}
