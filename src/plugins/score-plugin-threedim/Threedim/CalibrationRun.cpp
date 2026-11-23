#include "CalibrationRun.hpp"

#include <QDir>
#include <QFileInfo>
#include <QThreadPool>

#include <atomic>
#include <cstring>
#include <mutex>
#include <utility>

namespace Threedim
{
struct CalibrationRun::Pending
{
  std::atomic_bool cancel{};
  std::atomic<float> progress{};
  std::mutex mutex;
  std::optional<CalibrationPostProcess::Outcome> outcome;
};

CalibrationRun::Camera::~Camera() = default;

namespace
{
class TextureCamera final : public CalibrationRun::Camera
{
public:
  TextureCamera(Gfx::GfxContext& ctx, Gfx::port_index p)
      : m_capture{ctx, p}
  {
  }
  Gfx::TextureCapture::State state() const override { return m_capture.state(); }
  QString reason() const override { return m_capture.reason(); }
  void setProducer(Gfx::port_index p) override { m_capture.setProducer(p); }
  void request() override { m_capture.request(); }
  bool take(std::vector<float>& rgba, int& w, int& h) override
  {
    return m_capture.take(rgba, w, h);
  }
  int64_t frames() const override { return m_capture.frames(); }

private:
  Gfx::TextureCapture m_capture;
};

uint64_t sparseHash(const std::vector<float>& v) noexcept
{
  uint64_t h = 1469598103934665603ull;
  for(std::size_t i = 0; i < v.size(); i += 97)
  {
    uint32_t b;
    std::memcpy(&b, &v[i], 4);
    h = (h ^ b) * 1099511628211ull;
  }
  return h;
}
}

CalibrationRun::Display
CalibrationRun::multiWindow(std::function<score::gfx::MultiWindowNode*()> node)
{
  Display d;
  d.windows = [node] {
    auto* n = node ? node() : nullptr;
    return n ? int(n->windowOutputs().size()) : -1;
  };
  d.show = [node](Images images) {
    if(auto* n = node ? node() : nullptr)
      n->setCalibration(std::move(images));
  };
  d.clear = [node] {
    if(auto* n = node ? node() : nullptr; n && n->calibrating())
      n->clearCalibration();
  };
  return d;
}

CalibrationRun::CalibrationRun(Setup s)
    : m_setup{std::move(s)}
{
  auto& st = m_setup.settings;
  st.projectorWidth = std::max(1, m_setup.plan.projectorSize.width());
  st.projectorHeight = std::max(1, m_setup.plan.projectorSize.height());
  st.projectors = std::max(1, m_setup.plan.projectors);
  if(m_setup.originalAutoExposure)
    st.autoExposureAfter = *m_setup.originalAutoExposure;
  m_setup.run.autoSave.clear();
  auto& post = m_setup.post;
  post.names = m_setup.plan.names;
  post.outputs = m_setup.plan.outputs;
  post.viewer = st.mode == StructuredLightSession::ViewerCamera;
  post.lens = st.lens;
  post.fov = st.fov;
  post.cameraIntrinsics = st.cameraIntrinsics;
  post.worldFromCamera = st.worldFromCamera;
  if(!m_setup.executor)
    m_setup.executor = [](std::function<void()> job) {
      QThreadPool::globalInstance()->start(std::move(job));
    };
  if(!m_setup.openCamera)
    m_setup.openCamera = [gfx = m_setup.gfx](Gfx::port_index p) -> std::unique_ptr<Camera> {
      if(!gfx)
        return nullptr;
      return std::make_unique<TextureCamera>(*gfx, p);
    };
  m_session.setExecutor(m_setup.executor);
}

CalibrationRun::~CalibrationRun()
{
  if(m_pending)
    m_pending->cancel = true;
  m_session.cancelComputation();
  release();
}

void CalibrationRun::release()
{
  m_camera.reset();
  m_positions.reset();
  if(m_setup.setExposure)
    if(auto e = std::exchange(m_setup.originalExposure, std::nullopt))
      m_setup.setExposure(*e);
  if(m_setup.setAutoExposure)
    if(auto a = std::exchange(m_setup.originalAutoExposure, std::nullopt))
      m_setup.setAutoExposure(*a);
  if(m_setup.display.clear)
    m_setup.display.clear();
}

QString CalibrationRun::status() const
{
  return m_state == State::Capturing ? QString::fromStdString(m_session.status()) : m_status;
}

float CalibrationRun::progress() const noexcept
{
  switch(m_state)
  {
    case State::Capturing:
      return 0.8f * m_session.progress();
    case State::Blending:
      return (m_saveOnly ? 0.f : 0.8f)
             + 0.2f * (m_pending ? m_pending->progress.load() : 0.f);
    case State::Done:
      return 1.f;
    default:
      return 0.f;
  }
}

QStringList CalibrationRun::takeLog()
{
  return std::exchange(m_log, {});
}

void CalibrationRun::fail(const QString& why)
{
  m_status = why;
  m_log.push_back(why);
  m_state = State::Failed;
  if(m_pending)
    m_pending->cancel = true;
  release();
}

void CalibrationRun::apply(const StructuredLightSession::Events& ev)
{
  if(ev.exposure && m_setup.setExposure)
    m_setup.setExposure(*ev.exposure);
  if(ev.autoExposure && m_setup.setAutoExposure)
    m_setup.setAutoExposure(*ev.autoExposure);
}

void CalibrationRun::openCamera(double now)
{
  m_producer = m_setup.camera ? m_setup.camera() : std::nullopt;
  m_camera = m_producer ? m_setup.openCamera(*m_producer) : nullptr;
  if(!m_streaming && m_setup.keepStreaming)
  {
    m_setup.keepStreaming(true);
    m_streaming = true;
  }
  m_requested = false;
  m_rgba.clear();
  m_distinct = 0;
  m_openedAt = now;
}

bool CalibrationRun::start(double now)
{
  if(running())
    return false;
  const int windows = m_setup.display.windows ? m_setup.display.windows() : -1;
  if(windows < 0)
  {
    fail(tr("The output device is not running: connect it first."));
    return false;
  }
  if(!m_setup.plan.error.isEmpty())
  {
    fail(m_setup.plan.error);
    return false;
  }
  if(m_setup.plan.projectors <= 0 || m_setup.plan.names.isEmpty())
  {
    fail(tr("Nothing to calibrate: the device has no output."));
    return false;
  }
  for(int i : m_setup.plan.outputs)
    if(i >= windows)
    {
      fail(tr("Output %1 has no window.").arg(i + 1));
      return false;
    }
  openCamera(now);
  if(!m_camera)
  {
    fail(tr("The camera address is not a texture of a camera device."));
    return false;
  }
  m_reopens = 0;
  m_reopenAt = -1.;
  m_status = tr("Waiting for the camera");
  m_state = State::Opening;
  return true;
}

bool CalibrationRun::recompute(double)
{
  if(running())
    return false;
  if(m_setup.plan.names.isEmpty())
  {
    fail(tr("Nothing to recompute: the device has no output."));
    return false;
  }
  m_log.push_back(tr("Recomputing %1 output(s) from their measured maps").arg(m_setup.plan.names.size()));
  launch({}, false, false);
  return true;
}

void CalibrationRun::startSession(double now)
{
  StructuredLightSession::Events ev;
  if(!m_session.start(m_setup.settings, m_setup.run, now, ev))
  {
    fail(QString::fromStdString(m_session.status()));
    return;
  }
  apply(ev);
  if(m_setup.settings.mode == StructuredLightSession::DepthCamera)
  {
    const auto p = m_setup.positions ? m_setup.positions() : std::nullopt;
    m_positions = p ? m_setup.openCamera(*p) : nullptr;
    if(!m_positions)
      m_log.push_back(tr("Depth mode without a Positions address: the XYZ map is solved "
                         "from the camera intrinsics."));
  }
  m_shown.assign(std::max(0, m_setup.plan.projectors), -1);
  m_state = State::Capturing;
  show();
}

void CalibrationRun::show()
{
  const int windows = m_setup.display.windows ? m_setup.display.windows() : -1;
  if(windows < 0 || !m_setup.display.show)
    return;
  bool changed = false;
  const int n = std::min<int>(m_session.images(), int(m_shown.size()));
  for(int k = 0; k < n; k++)
    if(m_session.image(k).version != m_shown[k])
      changed = true;
  if(!changed)
    return;
  m_distinctAtChange = m_distinct;
  Images images(windows);
  const auto& sel = m_setup.plan.outputs;
  for(int k = 0; k < n; k++)
  {
    const auto& d = m_session.image(k);
    m_shown[k] = d.version;
    const int i = k < (int)sel.size() ? sel[k] : k;
    if(i < 0 || i >= windows)
      continue;
    auto img = std::make_shared<Image>();
    img->width = d.width;
    img->height = d.height;
    img->rgba = d.rgba;
    images[i] = std::move(img);
  }
  m_setup.display.show(std::move(images));
}

bool CalibrationRun::readCamera(double now)
{
  if(!m_camera)
    return false;
  if(!m_requested)
  {
    m_camera->request();
    m_requested = true;
  }
  if(!m_camera->take(m_rgba, m_rgbaW, m_rgbaH))
    return false;
  m_requested = false;
  const uint64_t h = sparseHash(m_rgba);
  if(h == m_hash && m_distinct > 0)
    return false;
  m_hash = h;
  m_distinct++;
  return true;
}

void CalibrationRun::tickOpening(double now)
{
  if(m_reopenAt >= 0.)
  {
    if(now < m_reopenAt)
      return;
    m_reopenAt = -1.;
    openCamera(now);
    if(!m_camera)
    {
      fail(tr("The camera went away."));
      return;
    }
  }
  if(m_camera->state() == Gfx::TextureCapture::State::Unavailable)
  {
    fail(tr("The camera cannot be read: %1").arg(m_camera->reason()));
    return;
  }
  readCamera(now);
  const QSize want = m_setup.expectedCameraSize;
  const bool wrongSize = want.isValid() && !want.isEmpty() && !m_rgba.empty()
                         && QSize(m_rgbaW, m_rgbaH) != want;
  if(!wrongSize && m_distinct >= uint64_t(std::max(1, m_setup.warmupFrames)))
  {
    m_log.push_back(tr("Camera: %1x%2").arg(m_rgbaW).arg(m_rgbaH));
    startSession(now);
    return;
  }
  if(!wrongSize && now - m_openedAt < m_setup.warmupMs)
    return;
  if(wrongSize && m_distinct < uint64_t(std::max(1, m_setup.warmupFrames)))
    return;

  const QString what
      = wrongSize ? tr("The camera delivers %1x%2 and its device is set to %3x%4")
                        .arg(m_rgbaW)
                        .arg(m_rgbaH)
                        .arg(want.width())
                        .arg(want.height())
                  : tr("The camera delivered %1 distinct image(s) in %2 s (%3 read back)")
                        .arg(m_distinct)
                        .arg(int(m_setup.warmupMs / 1000.))
                        .arg(m_camera->frames());
  if(m_reopens < m_setup.reopenDelaysMs.size() && m_setup.keepStreaming)
  {
    const double delay = m_setup.reopenDelaysMs[m_reopens++];
    m_log.push_back(tr("%1: reopening it in %2 s").arg(what).arg(delay / 1000., 0, 'f', 1));
    m_camera.reset();
    if(std::exchange(m_streaming, false))
      m_setup.keepStreaming(false);
    m_reopenAt = now + delay;
    return;
  }
  fail(wrongSize ? tr("%1: it did not open in its mode. Close what else uses it, or "
                      "reconnect it.")
                       .arg(what)
                 : tr("%1: is it streaming, or held by another program?").arg(what));
}

void CalibrationRun::tickCapture(double now)
{
  if(const auto p = m_setup.camera ? m_setup.camera() : std::nullopt; !p)
  {
    abort(tr("The camera went away"));
    return;
  }
  else if(!m_producer || *p != *m_producer)
  {
    m_producer = p;
    m_camera->setProducer(*p);
    m_requested = false;
    if(m_setup.keepStreaming)
      m_setup.keepStreaming(true);
    m_log.push_back(tr("The camera device was created again: following it"));
  }
  if(m_camera->state() == Gfx::TextureCapture::State::Unavailable)
  {
    abort(tr("The camera cannot be read: %1").arg(m_camera->reason()));
    return;
  }
  StructuredLightSession::Frame f;
  if(m_session.wantsCamera())
  {
    readCamera(now);
    if(!m_rgba.empty()
       && m_distinct >= m_distinctAtChange + uint64_t(std::max(0, m_setup.latencyFrames)))
      f.camera = CameraFrame{m_rgba.data(), m_rgbaW, m_rgbaH, m_distinct};
  }
  if(m_positions && m_session.wantsPositions())
  {
    if(!m_xyzRequested)
    {
      m_positions->request();
      m_xyzRequested = true;
    }
    if(m_positions->take(m_xyz, m_xyzW, m_xyzH))
      m_xyzRequested = false;
    if(!m_xyz.empty())
    {
      f.positions = m_xyz.data();
      f.positionsWidth = m_xyzW;
      f.positionsHeight = m_xyzH;
    }
  }
  StructuredLightSession::Events ev;
  m_session.tick(m_setup.settings, now, f, ev);
  apply(ev);
  show();

  if(ev.done)
  {
    m_log.push_back(QString::fromStdString(m_session.status()));
    m_camera.reset();
    m_positions.reset();
    launch(m_session.results(), true, false);
    return;
  }
  if(m_session.running())
    return;
  const auto& results = m_session.results();
  if(results.empty())
  {
    fail(QString::fromStdString(m_session.status()));
    return;
  }
  int ok = 0;
  for(std::size_t i = 0; i < results.size(); i++)
  {
    const auto& r = results[i];
    ok += r && r->ok;
    m_log.push_back(tr("Output %1: %2")
                        .arg(CalibrationPostProcess::label(m_setup.post, int(i)))
                        .arg(r ? QString::fromStdString(r->status) : QString{}));
  }
  m_status = tr("%1 of %2 outputs calibrated; the others failed, nothing was assigned (the "
                "calibrated ones are saved)")
                 .arg(ok)
                 .arg(int(results.size()));
  m_camera.reset();
  launch(results, true, true);
}

void CalibrationRun::launch(CalibrationPostProcess::Results results, bool fresh, bool saveOnly)
{
  m_saveOnly = saveOnly;
  if(!saveOnly)
  {
    m_status = m_setup.post.blend ? tr("Computing the warp and blend maps")
                                  : tr("Computing the maps");
    m_log.push_back(m_status);
  }
  m_state = State::Blending;
  m_pending = std::make_shared<Pending>();
  m_setup.executor([pending = m_pending, results = std::move(results), o = m_setup.post,
                    fresh, saveOnly]() mutable {
    CalibrationPostProcess::Outcome out;
    if(!fresh)
      results = CalibrationPostProcess::load(o.names, out.log);
    if(saveOnly)
    {
      CalibrationPostProcess::saveResults(results, o, out.log);
      pending->progress = 1.f;
    }
    else
    {
      auto r = CalibrationPostProcess::run(results, o, fresh, &pending->cancel, &pending->progress);
      r.log = out.log + r.log;
      out = std::move(r);
    }
    std::lock_guard l{pending->mutex};
    pending->outcome = std::move(out);
  });
}

void CalibrationRun::abort(const QString& why)
{
  if(m_state == State::Capturing)
  {
    StructuredLightSession::Events ev;
    m_session.abort(m_setup.settings, ev);
    apply(ev);
  }
  if(running() || m_state == State::Idle)
    fail(why);
}

void CalibrationRun::tick(double now)
{
  if(!running())
    return;
  if(m_state != State::Blending
     && (!m_setup.display.windows || m_setup.display.windows() < 0))
  {
    abort(tr("The output device went away"));
    return;
  }
  switch(m_state)
  {
    case State::Opening:
      tickOpening(now);
      break;
    case State::Capturing:
      tickCapture(now);
      break;
    case State::Blending: {
      std::optional<CalibrationPostProcess::Outcome> out;
      {
        std::lock_guard l{m_pending->mutex};
        out = std::exchange(m_pending->outcome, std::nullopt);
      }
      if(!out)
        return;
      m_log.append(out->log);
      if(m_saveOnly)
      {
        fail(m_status);
        return;
      }
      if(!out->ok)
      {
        fail(out->status);
        return;
      }
      m_status = out->status;
      m_outcome = std::move(out);
      m_state = State::Done;
      release();
      break;
    }
    default:
      break;
  }
}
}
