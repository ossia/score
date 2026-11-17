#include "StructuredLightSession.hpp"

#include <Threedim/FloatTextureIO.hpp>

#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace Threedim
{

StructuredLightSession::~StructuredLightSession()
{
  cancelComputation();
}

void StructuredLightSession::cancelComputation() noexcept
{
  if(m_cancel)
    m_cancel->store(true, std::memory_order_relaxed);
  if(!m_future.valid())
    return;
  try
  {
    std::thread([f = std::move(m_future)]() mutable { f.wait(); }).detach();
  }
  catch(...)
  {
    if(m_future.valid())
      m_future.wait();
  }
  m_future = {};
}

void StructuredLightSession::showPattern(
    const sl::GraySequence& seq, int projector, int projectors, int k, float level,
    const std::vector<std::array<float, 3>>& extraColors)
{
  if(m_shown == k && m_shownLevel == level && m_display.width == seq.width
     && m_display.height == seq.height && m_shownProjector == projector
     && m_shownProjectors == projectors && !m_display.idle)
    return;
  auto& d = m_display;
  d.width = seq.width;
  d.height = seq.height;
  d.rgba.resize(std::size_t(seq.width) * seq.height * 4);
  if(k >= seq.count())
  {
    const std::size_t e = std::size_t(k - seq.count());
    const auto c = e < extraColors.size() ? extraColors[e] : std::array<float, 3>{};
    uint8_t v[3];
    for(int i = 0; i < 3; i++)
      v[i] = uint8_t(std::lround(std::clamp(c[i], 0.f, 1.f) * 255.f));
    auto* px = d.rgba.data();
    for(std::size_t i = 0; i < std::size_t(seq.width) * seq.height; i++)
    {
      px[i * 4 + 0] = v[0];
      px[i * 4 + 1] = v[1];
      px[i * 4 + 2] = v[2];
      px[i * 4 + 3] = 255;
    }
  }
  else
    sl::renderPattern(seq, k, level, d.rgba.data());
  d.projector = projector;
  d.projectors = projectors;
  d.idle = false;
  d.version++;
  m_shown = k;
  m_shownIdle = -1;
  m_shownLevel = level;
  m_shownProjector = projector;
  m_shownProjectors = projectors;
}

void StructuredLightSession::showIdle(int w, int h, int idle, float level, int projectors)
{
  w = std::max(1, w);
  h = std::max(1, h);
  if(m_shown == -1 && m_shownIdle == idle && m_shownLevel == level && m_display.width == w
     && m_display.height == h && m_shownProjectors == projectors && m_display.idle)
    return;
  auto& d = m_display;
  d.width = w;
  d.height = h;
  d.rgba.resize(std::size_t(w) * h * 4);
  auto* px = d.rgba.data();
  if(idle == TestGrid)
    sl::renderTestGrid(w, h, level, px);
  else
  {
    const uint8_t c
        = idle == White ? uint8_t(std::lround(std::clamp(level, 0.f, 1.f) * 255.f)) : 0;
    for(std::size_t i = 0; i < std::size_t(w) * h; i++)
    {
      px[i * 4 + 0] = px[i * 4 + 1] = px[i * 4 + 2] = c;
      px[i * 4 + 3] = 255;
    }
  }
  d.projector = 0;
  d.projectors = projectors;
  d.idle = true;
  d.version++;
  m_shown = -1;
  m_shownIdle = idle;
  m_shownLevel = level;
  m_shownProjector = -1;
  m_shownProjectors = projectors;
}

void StructuredLightSession::renderCanvas(
    int W, int H, const std::string& layout, int count, uint8_t* dst) const
{
  for(std::size_t i = 0; i < std::size_t(W) * H; i++)
  {
    dst[i * 4 + 0] = dst[i * 4 + 1] = dst[i * 4 + 2] = 0;
    dst[i * 4 + 3] = 255;
  }
  const auto& d = m_display;
  if(d.width <= 0 || d.height <= 0 || d.rgba.empty())
    return;
  const auto rects = canvasRects(layout, count);
  for(int p = 0; p < count && p < int(rects.size()); p++)
  {
    if(!d.idle && p != d.projector)
      continue;
    const auto& r = rects[p];
    const int x0 = std::clamp(int(std::lround(r[0] * W)), 0, W);
    const int y0 = std::clamp(int(std::lround(r[1] * H)), 0, H);
    const int x1 = std::clamp(int(std::lround((r[0] + r[2]) * W)), 0, W);
    const int y1 = std::clamp(int(std::lround((r[1] + r[3]) * H)), 0, H);
    const double fx = double(d.width) / std::max(1., std::lround(r[2] * W) * 1.);
    const double fy = double(d.height) / std::max(1., std::lround(r[3] * H) * 1.);
    const int ox = int(std::lround(r[0] * W)), oy = int(std::lround(r[1] * H));
    for(int y = y0; y < y1; y++)
    {
      const int sy = std::clamp(int((y - oy + 0.5) * fy), 0, d.height - 1);
      for(int x = x0; x < x1; x++)
      {
        const int sx = std::clamp(int((x - ox + 0.5) * fx), 0, d.width - 1);
        std::memcpy(
            dst + (std::size_t(y) * W + x) * 4,
            d.rgba.data() + (std::size_t(sy) * d.width + sx) * 4, 4);
      }
    }
  }
}

void StructuredLightSession::note(const Result& r, bool accumulate)
{
  if(!r.ok)
    return;
  if(r.fromFile && r.crf)
    m_loadedCrf = r.crf;
  if(r.response && r.projectorIndex == 0)
    for(int c = 0; c < 3; c++)
      m_measuredGamma[c] = float(r.response->channels[c].gamma);
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

void StructuredLightSession::adopt(std::shared_ptr<Result> r, bool accumulate)
{
  r->projectorIndex = 0;
  m_results = {std::shared_ptr<const Result>(std::move(r))};
  m_resultVersion++;
  m_status = m_results[0]->status;
  note(*m_results[0], accumulate);
}

std::string StructuredLightSession::saveAll(const std::string& path) const
{
  std::string saved, failed;
  for(std::size_t i = 0; i < m_results.size(); i++)
  {
    const auto p = projectorPath(path, int(i), int(m_results.size()));
    const auto name = SaveFloatTexture::basePath(p);
    const auto& r = m_results[i];
    if(r && r->ok && !r->fromFile && saveFile(p, *r))
      saved += (saved.empty() ? "" : ", ") + name + ".json";
    else
      failed += (failed.empty() ? "" : ", ") + name
                + (!r || !r->ok    ? " (no result)"
                   : r->fromFile ? " (loaded from a file, not re-saved)"
                                 : " (cannot write)");
  }
  if(m_results.empty())
    failed = SaveFloatTexture::basePath(path) + " (no result)";
  return failed.empty()  ? "Saved " + saved
         : saved.empty() ? "Failed: " + failed
                         : "Saved " + saved + "; failed: " + failed;
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
  if(m_phase != Phase::Idle && s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
  }
  m_phase = Phase::Idle;
  m_pendingJobs.clear();
  m_job = Job{};
  m_wantCamera = false;
  m_wantPositions = false;
  m_status = status;
}

void StructuredLightSession::abort(const Settings& s, Events& ev, const std::string& status)
{
  if(m_phase == Phase::Idle)
    return;
  cancelComputation();
  stop(s, status, ev);
}

bool StructuredLightSession::start(const Settings& s, double now, Events& ev)
{
  if(m_phase != Phase::Idle)
    return false;
  m_seq = sl::GraySequence::make(s.projectorWidth, s.projectorHeight, s.minStripe);
  if(s.phaseShift)
    m_seq.enablePhase(s.phaseSteps, s.phasePeriod, s.projectorGamma);
  m_job = Job{};
  m_job.seq = m_seq;
  m_pendingJobs.clear();
  m_results.clear();
  m_projectors = std::max(1, s.projectors);
  if(!s.exposures.empty() && !(s.captureExposure > 0.f))
  {
    m_status = "Failed: an exposure bracket needs a Capture exposure to return the camera to";
    return false;
  }
  m_extras = makeExtras(s.responseLevels, s.compensation, s.exposures, 0, s.mixLevels);
  m_extraColors.clear();
  for(auto& e : m_extras)
    m_extraColors.push_back(e.rgb);
  if(s.crfFile != m_crfFileLoaded)
  {
    m_crfFileLoaded = s.crfFile;
    m_loadedCrf = m_crfFileLoaded.empty() ? nullptr : loadCameraResponse(m_crfFileLoaded);
  }
  m_settleMs = s.settleMs;
  if(s.captureExposure > 0.f)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
    m_settleMs = std::max(m_settleMs, s.exposureSettleMs);
  }
  if(s.captureExposure > 0.f || !s.exposures.empty())
    ev.autoExposure = false;
  m_projector = 0;
  m_rgbSum.clear();
  m_camW = m_camH = 0;
  m_k = 0;
  m_framesTaken = 0;
  m_frameSum.clear();
  m_phase = Phase::Settling;
  m_shownAt = now;
  m_wantCamera = true;
  m_wantPositions = s.mode == DepthCamera;
  showPattern(m_seq, 0, m_projectors, 0, s.level, m_extraColors);
  m_framesBefore = 0;
  m_framesTotal
      = frameCount()
        + (m_projectors - 1)
              * (m_seq.count()
                 + int(makeExtras(s.responseLevels, s.compensation, {}, 1, s.mixLevels)
                           .size()));
  m_status = "Capturing pattern 1 / " + std::to_string(frameCount());
  if(m_projectors > 1)
    m_status += " of projector 1 / " + std::to_string(m_projectors);
  if(m_seq.phaseSteps > 0)
    m_status += " (phase " + std::to_string(m_seq.phaseSteps) + " steps, period "
                + std::to_string(m_seq.period) + " px)";
  return true;
}

void StructuredLightSession::finishProjector(const Settings& s, double now, Events& ev)
{
  m_pendingJobs.push_back(std::move(m_job));
  m_job = Job{};
  if(m_projector + 1 < m_projectors)
  {
    m_framesBefore += frameCount();
    m_projector++;
    m_job.seq = m_seq;
    m_extras = makeExtras(s.responseLevels, s.compensation, {}, m_projector, s.mixLevels);
    m_extraColors.clear();
    for(auto& e : m_extras)
      m_extraColors.push_back(e.rgb);
    m_camW = m_camH = 0;
    m_k = 0;
    m_framesTaken = 0;
    m_frameSum.clear();
    m_rgbSum.clear();
    m_settleMs = s.settleMs;
    if(s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
    {
      ev.exposure = s.captureExposure;
      m_exposureSent = s.captureExposure;
      m_settleMs = std::max(m_settleMs, s.exposureSettleMs);
    }
    showPattern(m_seq, m_projector, m_projectors, 0, s.level, m_extraColors);
    m_shownAt = now;
    m_phase = Phase::Settling;
    m_wantCamera = true;
    m_wantPositions = s.mode == DepthCamera;
    m_status = "Capturing pattern 1 / " + std::to_string(frameCount()) + " of projector "
               + std::to_string(m_projector + 1) + " / " + std::to_string(m_projectors);
    return;
  }
  m_wantCamera = false;
  m_wantPositions = false;
  if(s.captureExposure > 0.f && m_exposureSent != s.captureExposure)
  {
    ev.exposure = s.captureExposure;
    m_exposureSent = s.captureExposure;
  }
  m_cancel = std::make_shared<std::atomic_bool>(false);
  for(auto& j : m_pendingJobs)
    j.cancel = m_cancel;
  try
  {
    m_future = std::async(
        std::launch::async,
        [jobs = std::move(m_pendingJobs)]() mutable -> std::vector<Result> {
      try
      {
        return computeAll(std::move(jobs));
      }
      catch(const std::exception& e)
      {
        Result r;
        r.status = std::string("Failed: ") + e.what();
        return {std::move(r)};
      }
      catch(...)
      {
        Result r;
        r.status = "Failed: unknown error";
        return {std::move(r)};
      }
    });
  }
  catch(const std::exception& e)
  {
    m_pendingJobs.clear();
    stop(s, std::string("Failed: ") + e.what(), ev);
    return;
  }
  m_pendingJobs.clear();
  m_phase = Phase::Computing;
  m_status = "Decoding";
  m_projector = 0;
  showIdle(s.projectorWidth, s.projectorHeight, s.idle, s.level, s.projectors);
}

bool StructuredLightSession::captureFrame(const Settings& s, const Frame& t)
{
  if(!t.rgba || t.width <= 0 || t.height <= 0)
    return false;
  if(m_camW == 0)
  {
    m_camW = t.width;
    m_camH = t.height;
    m_job.decoder.reset(m_camW, m_camH, m_seq);
    m_job.decoder.setContrast(s.contrast);
    m_job.decoder.setPhaseMinModulation(s.phaseMinModulation);
    const std::size_t n = std::size_t(m_camW) * m_camH;
    m_job.positions.assign(n * 3, 0.f);
    m_job.positionCount.assign(n, 0);
    m_job.zMin.assign(n, 1e30f);
    m_job.zMax.assign(n, -1e30f);
    m_job.positionFrames = 0;
  }
  if(t.width != m_camW || t.height != m_camH)
  {
    Events ignored;
    stop(s, "Aborted: the camera size changed during the capture", ignored);
    return false;
  }
  const std::size_t n = std::size_t(m_camW) * m_camH;
  if(m_frameSum.size() != n)
    m_frameSum.assign(n, 0.f);
  for(std::size_t i = 0; i < n; i++)
  {
    const float* p = t.rgba + i * 4;
    m_frameSum[i] += (p[0] + p[1] + p[2]) * (1.f / 3.f);
  }
  if(m_k <= 1 || m_k >= m_seq.count())
  {
    if(m_rgbSum.size() != n * 3)
      m_rgbSum.assign(n * 3, 0.f);
    for(std::size_t i = 0; i < n; i++)
      for(int c = 0; c < 3; c++)
        m_rgbSum[i * 3 + c] += t.rgba[i * 4 + c];
  }

  if(s.mode == DepthCamera && m_k <= 1 && t.positions && t.positionsWidth > 0
     && t.positionsHeight > 0)
  {
    const int pw = t.positionsWidth, ph = t.positionsHeight;
    m_job.positionFrames++;
    for(int y = 0; y < m_camH; y++)
    {
      const int py = std::min(ph - 1, int((y + 0.5) * ph / m_camH));
      for(int x = 0; x < m_camW; x++)
      {
        const int px = std::min(pw - 1, int((x + 0.5) * pw / m_camW));
        const float* p = t.positions + (std::size_t(py) * pw + px) * 4;
        if(!(p[3] > 0.f) || !(p[2] > 0.f) || !std::isfinite(p[0]) || !std::isfinite(p[1])
           || !std::isfinite(p[2]))
          continue;
        const std::size_t i = std::size_t(y) * m_camW + x;
        m_job.positions[i * 3] += p[0];
        m_job.positions[i * 3 + 1] += p[1];
        m_job.positions[i * 3 + 2] += p[2];
        m_job.positionCount[i]++;
        m_job.zMin[i] = std::min(m_job.zMin[i], p[2]);
        m_job.zMax[i] = std::max(m_job.zMax[i], p[2]);
      }
    }
  }
  return true;
}

void StructuredLightSession::tick(
    const Settings& s, double now, const Frame& frame, Events& ev)
{
  switch(m_phase)
  {
    case Phase::Idle:
      showIdle(s.projectorWidth, s.projectorHeight, s.idle, s.level, s.projectors);
      m_progress = m_results.empty() ? 0.f : 1.f;
      return;

    case Phase::Settling:
      if(now - m_shownAt >= m_settleMs)
      {
        m_phase = Phase::Capturing;
        m_framesTaken = 0;
        m_frameSum.clear();
      }
      break;

    case Phase::Capturing:
      if(now - m_lastFrameAt >= 30. && captureFrame(s, frame))
      {
        m_lastFrameAt = now;
        m_framesTaken++;
        if(m_framesTaken >= std::max(1, s.frames))
        {
          for(float& v : m_frameSum)
            v /= float(m_framesTaken);
          if(static const bool trace = qEnvironmentVariableIsSet("SCORE_SL_TRACE"); trace)
          {
            double mean = 0.;
            uint64_t hash = 1469598103934665603ull;
            for(float v : m_frameSum)
            {
              mean += v;
              uint32_t b;
              std::memcpy(&b, &v, 4);
              hash = (hash ^ b) * 1099511628211ull;
            }
            qDebug(
                "[sl-trace] pattern %d mean %.5f hash %016llx", m_k,
                mean / std::max<std::size_t>(1, m_frameSum.size()),
                (unsigned long long)hash);
          }
          if(m_k == 0)
            m_job.whiteMax = *std::max_element(m_frameSum.begin(), m_frameSum.end());
          if(m_k == 0 && m_job.whiteMax <= 1e-6f)
          {
            stop(
                s,
                "Failed: the camera image was black during the white pattern: the "
                "camera delivered no frame (is it streaming?)",
                ev);
            return;
          }
          const bool extra = m_k >= m_seq.count();
          if((m_k <= 1 || extra) && m_rgbSum.size() == m_frameSum.size() * 3)
          {
            for(float& v : m_rgbSum)
              v /= float(m_framesTaken);
            if(!extra)
              (m_k == 0 ? m_job.whiteRGB : m_job.blackRGB) = std::move(m_rgbSum);
            else if(auto& e = m_extras[m_k - m_seq.count()]; e.full)
              e.rgbCapture = std::move(m_rgbSum);
            else
            {
              const int S = std::max(1, m_job.extraStride);
              const int sw = (m_camW + S - 1) / S, shh = (m_camH + S - 1) / S;
              e.rgbCapture.resize(std::size_t(sw) * shh * 3);
              for(int y = 0; y < shh; y++)
                for(int x = 0; x < sw; x++)
                  for(int c = 0; c < 3; c++)
                    e.rgbCapture[(std::size_t(y) * sw + x) * 3 + c]
                        = m_rgbSum[(std::size_t(y * S) * m_camW + x * S) * 3 + c];
            }
          }
          m_rgbSum.clear();
          if(!extra)
            m_job.decoder.add(m_k, m_frameSum);
          m_k++;
          if(m_k >= frameCount())
          {
            m_wantCamera = false;
            m_wantPositions = false;
            m_job.worldFromCamera = s.worldFromCamera;
            const auto k = s.cameraIntrinsics;
            m_job.cameraIntrinsics = k;
            if(!(k[0] > 0.f) || !(k[1] > 0.f))
            {
              const float f
                  = float(m_camW / (2. * std::tan(s.fov * 0.5 * M_PI / 180.)));
              m_job.cameraIntrinsics = {f, f, (m_camW - 1) * 0.5f, (m_camH - 1) * 0.5f};
            }
            m_job.mode = s.mode;
            m_job.minSignal = s.minSignal;
            m_job.minBits = s.minBits;
            m_job.holeRadius = s.holeRadius;
            m_job.depthJump = s.depthJump;
            m_job.ransac = s.ransac;
            resolveIntrinsics(s, m_job);
            m_job.surface = s.surface;
            m_job.maxPlanes = s.maxPlanes;
            m_job.planeThreshold = s.planeThreshold;
            m_job.cameraGamma = s.cameraGamma;
            m_job.projectorIndex = m_projector;
            m_job.extras = std::move(m_extras);
            m_extras.clear();
            m_job.crf = m_loadedCrf;
            m_job.compGamut = s.compGamut;
            m_job.compStrength = s.compStrength;
            m_job.compGradient = s.compGradient;
            m_job.mixLevels
                = s.compensation && s.responseLevels > 0 ? s.mixLevels : 0;
            m_job.compBlack = s.compBlack;
            finishProjector(s, now, ev);
          }
          else
          {
            showPattern(m_seq, m_projector, m_projectors, m_k, s.level, m_extraColors);
            m_shownAt = now;
            m_settleMs = s.settleMs;
            if(m_k >= m_seq.count())
              if(const auto& e = m_extras[m_k - m_seq.count()];
                 e.exposure > 0.f && e.exposure != m_exposureSent)
              {
                ev.exposure = e.exposure;
                m_exposureSent = e.exposure;
                m_settleMs = std::max(m_settleMs, s.exposureSettleMs);
              }
            m_phase = Phase::Settling;
            m_wantPositions = s.mode == DepthCamera && m_k <= 1;
            m_status = "Capturing pattern " + std::to_string(m_k + 1) + " / "
                       + std::to_string(frameCount());
            if(m_projectors > 1)
              m_status += " of projector " + std::to_string(m_projector + 1) + " / "
                          + std::to_string(m_projectors);
          }
        }
      }
      break;

    case Phase::Computing:
      if(m_future.valid()
         && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      {
        auto all = m_future.get();
        m_results.clear();
        for(auto& r : all)
          m_results.push_back(std::make_shared<Result>(std::move(r)));
        m_resultVersion++;
        std::string status;
        bool ok = !m_results.empty();
        for(std::size_t i = m_results.size(); i-- > 0;)
        {
          const auto& r = *m_results[i];
          note(r, s.accumulateIntrinsics);
          ok &= r.ok;
          if(m_results.size() > 1)
            status = "P" + std::to_string(i + 1) + ": " + r.status
                     + (status.empty() ? "" : " | " + status);
          else
            status = r.status;
        }
        m_status = status;
        m_phase = Phase::Idle;
        const bool any = std::any_of(
            m_results.begin(), m_results.end(), [](const auto& r) { return r && r->ok; });
        if(any && s.autoSave && !s.savePath.empty())
          m_status = status + " | " + saveAll(s.savePath);
        if(ok)
          ev.done = true;
      }
      break;
  }

  const float within
      = m_phase == Phase::Capturing ? float(m_framesTaken) / std::max(1, s.frames) : 0.f;
  m_progress = m_phase == Phase::Computing
                   ? 0.99f
                   : std::min(
                       0.99f,
                       (m_framesBefore + m_k + within) / float(std::max(1, m_framesTotal)));
}
}
