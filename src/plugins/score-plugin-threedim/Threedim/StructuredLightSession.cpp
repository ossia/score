#include "StructuredLightSession.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace Threedim
{
struct StructuredLightSession::Outbox
{
  struct Computed
  {
    uint64_t generation{};
    int projector{};
    Result result;
  };

  std::mutex mutex;
  std::vector<Computed> computed;
  std::optional<std::string> saved;
  std::optional<Result> loaded;
  std::atomic<int> inflight{};
};

StructuredLightSession::StructuredLightSession()
    : m_outbox{std::make_shared<Outbox>()}
    , m_images(1)
{
}

StructuredLightSession::~StructuredLightSession()
{
  cancelComputation();
}

void StructuredLightSession::cancelComputation() noexcept
{
  if(m_cancel)
    m_cancel->store(true, std::memory_order_relaxed);
  m_generation++;
}

void StructuredLightSession::post(std::function<void()> job)
{
  m_outbox->inflight++;
  auto run = [job = std::move(job), box = std::weak_ptr<Outbox>{m_outbox}] {
    job();
    if(auto b = box.lock())
      b->inflight--;
  };
  if(m_executor)
    m_executor(std::move(run));
  else
    run();
}

bool StructuredLightSession::busy() const noexcept
{
  return m_outbox->inflight.load() > 0;
}

bool StructuredLightSession::wantsCamera() const noexcept
{
  return m_phase == Phase::Capturing && m_capture.wantsFrame();
}

bool StructuredLightSession::wantsPositions() const noexcept
{
  return wantsCamera() && m_job.mode == DepthCamera && m_k <= 1;
}

const StructuredLightSession::Display& StructuredLightSession::image(int p) const noexcept
{
  static const Display none;
  return p >= 0 && p < int(m_images.size()) ? m_images[p] : none;
}

void StructuredLightSession::showPattern(int k, float level)
{
  if(m_shown == k && m_shownLevel == level && !m_images[m_projector].idle
     && m_images[m_projector].width == m_seq.width
     && m_images[m_projector].height == m_seq.height)
    return;
  const std::size_t n = std::size_t(m_seq.width) * m_seq.height;
  for(int p = 0; p < int(m_images.size()); p++)
  {
    auto& d = m_images[p];
    const bool lit = p == m_projector;
    if(!lit && !d.idle && d.width == m_seq.width && d.height == m_seq.height
       && d.projector == -1)
      continue;
    d.width = m_seq.width;
    d.height = m_seq.height;
    d.projectors = m_projectors;
    d.idle = false;
    d.projector = lit ? p : -1;
    d.rgba.resize(n * 4);
    if(!lit)
    {
      for(std::size_t i = 0; i < n; i++)
      {
        d.rgba[i * 4 + 0] = d.rgba[i * 4 + 1] = d.rgba[i * 4 + 2] = 0;
        d.rgba[i * 4 + 3] = 255;
      }
    }
    else
      sl::renderPattern(m_seq, k, level, d.rgba.data());
    d.version++;
  }
  m_shown = k;
  m_shownIdle = -1;
  m_shownLevel = level;
}

void StructuredLightSession::showIdle(const Settings& s)
{
  const int w = std::max(1, s.projectorWidth), h = std::max(1, s.projectorHeight);
  const int count = std::max(1, s.projectors);
  if(int(m_images.size()) != count)
    m_images.resize(count);
  const bool same = std::all_of(m_images.begin(), m_images.end(), [&](const Display& d) {
    return d.idle && d.width == w && d.height == h && d.projectors == count;
  });
  if(m_shown == -1 && m_shownIdle == s.idle && m_shownLevel == s.level && same)
    return;
  const std::size_t n = std::size_t(w) * h;
  for(int p = 0; p < count; p++)
  {
    auto& d = m_images[p];
    d.width = w;
    d.height = h;
    d.rgba.resize(n * 4);
    if(s.idle == TestGrid)
      sl::renderTestGrid(w, h, s.level, d.rgba.data());
    else
    {
      const uint8_t c
          = s.idle == White ? uint8_t(std::lround(std::clamp(s.level, 0.f, 1.f) * 255.f)) : 0;
      for(std::size_t i = 0; i < n; i++)
      {
        d.rgba[i * 4 + 0] = d.rgba[i * 4 + 1] = d.rgba[i * 4 + 2] = c;
        d.rgba[i * 4 + 3] = 255;
      }
    }
    d.projector = p;
    d.projectors = count;
    d.idle = true;
    d.version++;
  }
  m_shown = -1;
  m_shownIdle = s.idle;
  m_shownLevel = s.level;
}

void StructuredLightSession::note(const Result& r, bool accumulate)
{
  if(!r.ok)
    return;
  if(!(r.intrinsics[0] > 0.f) || r.mode != DepthCamera)
    return;
  if(r.fromFile)
  {
    m_loadedK = r.intrinsics;
    return;
  }
  if(r.intrinsicsLocked)
    return;
  m_lastSolvedK = r.intrinsics;
  if(!accumulate)
    return;
  m_solvedKs.push_back(r.intrinsics);
  if(m_solvedKs.size() > 8)
    m_solvedKs.erase(m_solvedKs.begin());
  for(int c = 0; c < 4; c++)
  {
    std::vector<float> v;
    for(auto& k : m_solvedKs)
      v.push_back(k[c]);
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    float med = v[v.size() / 2];
    if(v.size() % 2 == 0)
      med = 0.5f * (med + *std::max_element(v.begin(), v.begin() + v.size() / 2));
    m_accumulatedK[c] = med;
  }
}

void StructuredLightSession::load(const QString& path)
{
  post([path, box = std::weak_ptr<Outbox>{m_outbox}] {
    auto r = loadFile(path);
    if(auto b = box.lock())
    {
      std::lock_guard l{b->mutex};
      b->loaded = std::move(r);
    }
  });
}

void StructuredLightSession::save(const QString& path)
{
  if(m_results.empty())
  {
    m_status = "Failed: no result to save";
    return;
  }
  post([path, results = m_results, box = std::weak_ptr<Outbox>{m_outbox}] {
    std::string saved, failed;
    for(std::size_t i = 0; i < results.size(); i++)
    {
      const auto& r = results[i];
      const QString p = projectorPath(path, int(i));
      const std::string name = p.toStdString();
      QString err;
      if(r && r->ok && !r->fromFile && saveFile(p, *r, &err))
        saved += (saved.empty() ? "" : ", ") + name + ".json";
      else
        failed += (failed.empty() ? "" : ", ") + name
                  + (!r || !r->ok    ? " (no result)"
                     : r->fromFile ? " (loaded from a file, not saved again)"
                                   : " (" + err.toStdString() + ")");
    }
    auto status = failed.empty()  ? "Saved " + saved
                  : saved.empty() ? "Failed: " + failed
                                  : "Saved " + saved + "; failed: " + failed;
    if(auto b = box.lock())
    {
      std::lock_guard l{b->mutex};
      b->saved = std::move(status);
    }
  });
}

void StructuredLightSession::resolveIntrinsics(const Settings& s, Job& job) const
{
  job.intrinsicsMode = s.intrinsicsMode;
  job.lockK = false;
  job.kNote.clear();
  if(job.intrinsicsMode == SolveIntrinsics)
    return;

  std::array<float, 4> k{};
  if(job.intrinsicsMode == LockIntrinsics)
  {
    k = s.lockedIntrinsics;
    if(!(k[0] > 0.f && k[1] > 0.f) && s.throwRatio > 0.f)
    {
      const float W = float(m_seq.width), H = float(m_seq.height);
      const float f = s.throwRatio * W;
      k = {f, f, (W - 1.f) / 2.f - s.lensShift[0] * W, (H - 1.f) / 2.f + s.lensShift[1] * H};
    }
  }
  else
  {
    if(m_loadedK[0] > 0.f)
      k = m_loadedK;
    else if(m_lastSolvedK[0] > 0.f)
      k = m_lastSolvedK;
  }
  if(k[0] > 0.f && k[1] > 0.f)
  {
    job.lockK = true;
    job.lockedK = k;
  }
  else
  {
    job.kNote = job.intrinsicsMode == LockIntrinsics
                    ? "warning: Lock without Locked intrinsics nor Throw ratio, solved "
                      "instead"
                    : "warning: Lock to loaded without a loaded or solved calibration, "
                      "solved instead";
  }
}

void StructuredLightSession::stop(const Settings& s, const std::string& status, Events& ev)
{
  if(m_phase == Phase::Capturing)
  {
    if(s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
    {
      ev.exposure = s.captureExposure;
      m_exposureSent = s.captureExposure;
    }
    ev.autoExposure = s.autoExposureAfter;
  }
  cancelComputation();
  m_phase = Phase::Idle;
  m_job = Job{};
  m_capture.reset();
  m_status = status;
}

void StructuredLightSession::abort(const Settings& s, Events& ev, const std::string& status)
{
  if(m_phase == Phase::Idle)
    return;
  stop(s, status, ev);
}

bool StructuredLightSession::start(const Settings& s, const Run& run, double now, Events& ev)
{
  if(m_phase != Phase::Idle)
    return false;
  m_seq = sl::GraySequence::make(s.projectorWidth, s.projectorHeight, s.minStripe);
  if(s.phaseShift)
    m_seq.enablePhase(s.phaseSteps, s.phasePeriod, s.projectorGamma);
  m_generation++;
  m_cancel = std::make_shared<std::atomic_bool>(false);
  m_projectors = std::max(1, s.projectors);
  m_images.resize(m_projectors);
  m_runResults.assign(m_projectors, std::nullopt);
  m_submitted = m_pending = 0;
  m_results.clear();
  m_resultVersion++;
  m_autoSave = run.autoSave;
  m_latency = 0.;
  m_capture.reset();
  m_projector = 0;
  m_framesBefore = 0;
  m_framesTotal = m_projectors * m_seq.count();
  ev.autoExposure = false;
  if(s.captureExposure > 0.f)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
  }
  m_linear = PatternCapture::gammaTable(s.cameraGamma);
  m_phase = Phase::Capturing;
  beginProjector(s, now);
  beginPattern(s, now, ev);
  return true;
}

void StructuredLightSession::beginProjector(const Settings& s, double now)
{
  m_k = 0;
  m_job = Job{};
  m_job.seq = m_seq;
  m_job.cancel = m_cancel;
  m_job.mode = s.mode;
  m_job.projectorIndex = m_projector;
}

void StructuredLightSession::beginPattern(const Settings& s, double now, Events& ev)
{
  showPattern(m_k, s.level);
  PatternCapture::Options o;
  o.settleMs = std::max(s.settleMs, 1.25 * m_latency);
  o.frames = s.frames;
  o.timeoutMs = std::max(s.timeoutMs, 2. * m_latency);
  o.requireChange = m_projector > 0 || m_k > 0;
  o.darkerThan = m_k == 1 ? 0.9f * m_whiteMean : -1.f;
  o.linearise = &m_linear;
  o.rgb = m_k <= 1;
  o.clipLevel = 0.985f;
  if(m_k == 0 && s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
    o.settleMs = std::max(o.settleMs, s.exposureSettleMs);
  }
  m_capture.begin(now, o);
  m_status = "Capturing pattern " + std::to_string(m_k + 1) + " / "
             + std::to_string(frameCount());
  if(m_projectors > 1)
    m_status += " of projector " + std::to_string(m_projector + 1) + " / "
                + std::to_string(m_projectors);
  if(m_k == 0 && m_seq.phaseSteps > 0)
    m_status += " (phase " + std::to_string(m_seq.phaseSteps) + " steps, period "
                + std::to_string(m_seq.period) + " px)";
}

void StructuredLightSession::accumulatePositions(const Settings& s, const Frame& t)
{
  if(s.mode != DepthCamera || m_k > 1 || !t.positions || t.positionsWidth <= 0
     || t.positionsHeight <= 0)
    return;
  const int cw = m_capture.width(), ch = m_capture.height();
  const int pw = t.positionsWidth, ph = t.positionsHeight;
  m_job.positionFrames++;
  for(int y = 0; y < ch; y++)
  {
    const int py = std::min(ph - 1, int((y + 0.5) * ph / ch));
    for(int x = 0; x < cw; x++)
    {
      const int px = std::min(pw - 1, int((x + 0.5) * pw / cw));
      const float* p = t.positions + (std::size_t(py) * pw + px) * 4;
      if(!(p[3] > 0.f) || !(p[2] > 0.f) || !std::isfinite(p[0]) || !std::isfinite(p[1])
         || !std::isfinite(p[2]))
        continue;
      const std::size_t i = std::size_t(y) * cw + x;
      m_job.positions[i * 3] += p[0];
      m_job.positions[i * 3 + 1] += p[1];
      m_job.positions[i * 3 + 2] += p[2];
      m_job.positionCount[i]++;
      m_job.zMin[i] = std::min(m_job.zMin[i], p[2]);
      m_job.zMax[i] = std::max(m_job.zMax[i], p[2]);
    }
  }
}

void StructuredLightSession::submit(Job job)
{
  m_pending++;
  m_submitted++;
  post([job = std::make_shared<Job>(std::move(job)), gen = m_generation,
        box = std::weak_ptr<Outbox>{m_outbox}]() mutable {
    Outbox::Computed out;
    out.generation = gen;
    out.projector = job->projectorIndex;
    try
    {
      out.result = compute(std::move(*job));
    }
    catch(const std::exception& e)
    {
      out.result = {};
      out.result.status = std::string("Failed: ") + e.what();
    }
    job.reset();
    if(auto b = box.lock())
    {
      std::lock_guard l{b->mutex};
      b->computed.push_back(std::move(out));
    }
  });
}

void StructuredLightSession::endProjector(
    const Settings& s, double now, Events& ev, std::string failed)
{
  auto& job = m_job;
  job.failed = std::move(failed);
  job.worldFromCamera = s.worldFromCamera;
  const auto k = s.cameraIntrinsics;
  job.cameraIntrinsics = k;
  job.lens = s.lens;
  const int cw = job.decoder.width(), ch = job.decoder.height();
  if(!(k[0] > 0.f) || !(k[1] > 0.f))
  {
    const auto K = CameraLens::intrinsicsFromFov(s.lens.model, s.fov, cw, ch);
    job.cameraIntrinsics = {float(K[0]), float(K[1]), float(K[2]), float(K[3])};
  }
  job.minSignal = s.minSignal;
  job.minBits = s.minBits;
  job.holeRadius = s.holeRadius;
  job.depthJump = s.depthJump;
  job.ransac = s.ransac;
  resolveIntrinsics(s, job);
  job.surface = s.surface;
  job.maxPlanes = s.maxPlanes;
  job.planeThreshold = s.planeThreshold;
  job.cameraGamma = s.cameraGamma;
  job.keepPixels = s.keepPixels;
  job.latencyMs = m_latency;

  submit(std::move(job));
  m_job = Job{};

  if(m_projector + 1 < m_projectors)
  {
    m_framesBefore += frameCount();
    m_projector++;
    beginProjector(s, now);
    beginPattern(s, now, ev);
    return;
  }
  if(s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
  }
  ev.autoExposure = s.autoExposureAfter;
  m_capture.reset();
  m_phase = Phase::Computing;
  m_status = "Decoding";
  m_projector = 0;
  showIdle(s);
  if(m_pending == 0)
    finishRun(s, ev);
}

void StructuredLightSession::finishRun(const Settings& s, Events& ev)
{
  std::vector<Result> all;
  for(auto& r : m_runResults)
  {
    if(r)
      all.push_back(std::move(*r));
    else
    {
      Result failed;
      failed.status = "Failed: not computed";
      all.push_back(std::move(failed));
    }
  }
  m_runResults.clear();
  m_results.clear();
  std::string status;
  bool any = false;
  for(std::size_t i = 0; i < all.size(); i++)
  {
    note(all[i], s.accumulateIntrinsics);
    any |= all[i].ok;
    if(all.size() > 1)
      status += (i ? " | P" : "P") + std::to_string(i + 1) + ": " + all[i].status;
    else
      status = all[i].status;
    m_results.push_back(std::make_shared<const Result>(std::move(all[i])));
  }
  m_resultVersion++;
  m_status = status;
  m_phase = Phase::Idle;
  if(any)
  {
    ev.done = true;
    if(!m_autoSave.isEmpty())
      save(m_autoSave);
  }
}

void StructuredLightSession::collect(const Settings& s, Events& ev)
{
  std::vector<Outbox::Computed> computed;
  std::optional<std::string> saved;
  std::optional<Result> loaded;
  {
    std::lock_guard l{m_outbox->mutex};
    computed.swap(m_outbox->computed);
    saved.swap(m_outbox->saved);
    loaded.swap(m_outbox->loaded);
  }
  if(saved)
    m_status = *saved;
  if(loaded)
  {
    m_status = loaded->status;
    if(loaded->ok && m_phase == Phase::Idle)
    {
      note(*loaded, s.accumulateIntrinsics);
      m_results = {std::make_shared<const Result>(std::move(*loaded))};
      m_resultVersion++;
    }
  }
  for(auto& c : computed)
  {
    if(c.generation != m_generation || c.projector < 0
       || c.projector >= int(m_runResults.size()))
      continue;
    m_pending--;
    m_runResults[c.projector] = std::move(c.result);
  }
  if(m_phase == Phase::Computing && m_pending == 0)
    finishRun(s, ev);
}

void StructuredLightSession::tick(
    const Settings& s, double now, const Frame& frame, Events& ev)
{
  collect(s, ev);
  switch(m_phase)
  {
    case Phase::Idle:
      showIdle(s);
      m_progress = m_results.empty() ? 0.f : 1.f;
      return;
    case Phase::Computing:
      break;
    case Phase::Capturing:
    {
      const auto state = m_capture.feed(now, frame.camera);
      if(m_capture.accepted())
      {
        if(m_job.decoder.width() == 0)
        {
          const int cw = m_capture.width(), ch = m_capture.height();
          m_job.decoder.reset(cw, ch, m_seq);
          m_job.decoder.setContrast(s.contrast);
          m_job.decoder.setPhaseMinModulation(s.phaseMinModulation);
          const std::size_t n = std::size_t(cw) * ch;
          if(s.mode == DepthCamera)
          {
            m_job.positions.assign(n * 3, 0.f);
            m_job.positionCount.assign(n, 0);
            m_job.zMin.assign(n, 1e30f);
            m_job.zMax.assign(n, -1e30f);
          }
        }
        accumulatePositions(s, frame);
      }
      const std::string where = "pattern " + std::to_string(m_k + 1) + " of projector "
                                + std::to_string(m_projector + 1);
      switch(state)
      {
        case PatternCapture::State::NoFrame:
          stop(
              s,
              "Failed: no camera frame for " + std::to_string(int(s.timeoutMs))
                  + " ms at " + where + " (is the camera streaming?)",
              ev);
          return;
        case PatternCapture::State::SizeChanged:
          stop(s, "Aborted: the camera size changed during the capture", ev);
          return;
        case PatternCapture::State::Unchanged:
          endProjector(
              s, now, ev,
              m_k <= 1 ? "Failed: the camera does not see projector "
                             + std::to_string(m_projector + 1)
                             + ": its patterns do not change the image"
                       : "Failed: the camera image stopped changing at " + where
                             + " (frozen camera?)");
          break;
        case PatternCapture::State::Done:
        {
          if(m_k == 0)
          {
            m_whiteMean = m_capture.mean();
            m_job.whiteRGB = std::move(m_capture.rgb());
          }
          else if(m_k == 1)
          {
            m_latency = std::max(m_latency, m_capture.latency());
            m_job.blackRGB = std::move(m_capture.rgb());
          }
          {
            m_job.decoder.markSaturated(m_capture.clipped());
            if(!m_job.decoder.add(m_k, m_capture.luminance()))
            {
              endProjector(s, now, ev, "Failed: " + m_job.decoder.error());
              break;
            }
          }
          m_k++;
          if(m_k >= frameCount())
            endProjector(s, now, ev, {});
          else
            beginPattern(s, now, ev);
          break;
        }
        default:
          break;
      }
      break;
    }
  }

  const float within = m_phase == Phase::Capturing && m_capture.state()
                                                          == PatternCapture::State::Capturing
                           ? 0.5f
                           : 0.f;
  m_progress = m_phase == Phase::Computing
                   ? 0.99f
                   : std::min(
                       0.99f,
                       (m_framesBefore + m_k + within) / float(std::max(1, m_framesTotal)));
}
}
