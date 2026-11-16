#include "CalibrationRun.hpp"

#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/EquirectWarp.hpp>

#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <chrono>
#include <utility>

namespace Threedim
{
CalibrationRun::CalibrationRun(Setup s)
    : m_setup{std::move(s)}
{
  auto& st = m_setup.settings;
  st.projectorWidth = std::max(1, m_setup.plan.projectorSize.width());
  st.projectorHeight = std::max(1, m_setup.plan.projectorSize.height());
  st.projectors = std::max(1, m_setup.plan.projectors);
  if(!m_setup.plan.names.isEmpty())
    st.savePath = QString(m_setup.plan.names[0] + QStringLiteral(".json")).toStdString();
  st.autoSave = true;
}

CalibrationRun::~CalibrationRun()
{
  m_session.cancelComputation();
  release();
}

void CalibrationRun::release()
{
  m_camera.reset();
  m_positions.reset();
  if(m_setup.node)
    if(auto* n = m_setup.node(); n && n->calibrating())
      n->clearCalibration();
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
      return m_session.progress();
    case State::Blending:
      return 0.99f;
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
  release();
}

bool CalibrationRun::start(double now)
{
  if(running())
    return false;
  auto* node = m_setup.node ? m_setup.node() : nullptr;
  if(!node || !m_setup.gfx)
  {
    fail(QObject::tr("The output device is not running: connect it first."));
    return false;
  }
  if(m_setup.plan.projectors <= 0 || m_setup.plan.names.isEmpty())
  {
    fail(QObject::tr("Nothing to calibrate: the device has no output."));
    return false;
  }
  if(m_setup.camera.node < 0)
  {
    fail(QObject::tr("The camera address is not a texture of a camera device."));
    return false;
  }
  QDir{}.mkpath(QFileInfo{m_setup.plan.names[0]}.absolutePath());

  m_camera = std::make_unique<Gfx::TextureCapture>(*m_setup.gfx, m_setup.camera);
  if(m_setup.settings.mode == StructuredLightSession::DepthCamera
     && m_setup.positions.node >= 0)
    m_positions = std::make_unique<Gfx::TextureCapture>(*m_setup.gfx, m_setup.positions);
  else if(m_setup.settings.mode == StructuredLightSession::DepthCamera)
    m_log.push_back(QObject::tr("Depth mode without a Positions address: the XYZ map stays "
                                "empty."));

  StructuredLightSession::Events ev;
  if(!m_session.start(m_setup.settings, now, ev))
  {
    fail(QString::fromStdString(m_session.status()));
    return false;
  }
  if(ev.exposure && m_setup.setExposure)
    m_setup.setExposure(*ev.exposure);
  if(ev.autoExposure && m_setup.setAutoExposure)
    m_setup.setAutoExposure(*ev.autoExposure);
  m_shownVersion = -1;
  m_state = State::Capturing;
  show(*node);
  return true;
}

void CalibrationRun::show(score::gfx::MultiWindowNode& node)
{
  const auto& d = m_session.display();
  if(d.version == m_shownVersion && node.calibrating())
    return;
  m_shownVersion = d.version;
  auto img = std::make_shared<score::gfx::MultiWindowNode::CalibrationImage>();
  img->width = d.width;
  img->height = d.height;
  img->rgba = d.rgba;
  const int windows = int(node.windowOutputs().size());
  std::vector<std::shared_ptr<const score::gfx::MultiWindowNode::CalibrationImage>> images(
      windows);
  for(int i = 0; i < windows; i++)
    if(d.idle ? i < m_setup.plan.projectors : i == d.projector)
      images[i] = img;
  node.setCalibration(std::move(images));
}

void CalibrationRun::abort(const QString& why)
{
  if(m_state == State::Capturing)
  {
    StructuredLightSession::Events ev;
    m_session.abort(m_setup.settings, ev);
    if(ev.exposure && m_setup.setExposure)
      m_setup.setExposure(*ev.exposure);
  }
  if(running() || m_state == State::Idle)
    fail(why);
}

void CalibrationRun::tick(double now)
{
  if(!running())
    return;
  auto* node = m_setup.node ? m_setup.node() : nullptr;
  if(!node)
  {
    abort(QObject::tr("The output device went away"));
    return;
  }

  if(m_state == State::Blending)
  {
    if(m_blend.valid()
       && m_blend.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
      const auto r = m_blend.get();
      m_log.push_back(QObject::tr("Projector Blend Maps: %1").arg(QString::fromStdString(r.status)));
      m_status = QObject::tr("Done");
      m_state = State::Done;
      release();
    }
    return;
  }

  if(m_camera)
    m_camera->take(m_rgba, m_rgbaW, m_rgbaH);
  if(m_positions)
    m_positions->take(m_xyz, m_xyzW, m_xyzH);
  StructuredLightSession::Frame f;
  if(m_session.wantsCamera() && !m_rgba.empty())
  {
    f.rgba = m_rgba.data();
    f.width = m_rgbaW;
    f.height = m_rgbaH;
  }
  if(m_session.wantsPositions() && !m_xyz.empty())
  {
    f.positions = m_xyz.data();
    f.positionsWidth = m_xyzW;
    f.positionsHeight = m_xyzH;
  }

  StructuredLightSession::Events ev;
  m_session.tick(m_setup.settings, now, f, ev);
  if(ev.exposure && m_setup.setExposure)
    m_setup.setExposure(*ev.exposure);
  if(ev.autoExposure && m_setup.setAutoExposure)
    m_setup.setAutoExposure(*ev.autoExposure);
  show(*node);

  if(ev.done)
  {
    m_log.push_back(QString::fromStdString(m_session.status()));
    if(m_setup.canvas.projection == Gfx::CanvasProjection::Equirectangular
       && m_setup.settings.mode == StructuredLightSession::ViewerCamera)
    {
      const auto& results = m_session.results();
      int written = 0;
      for(std::size_t i = 0; i < results.size() && i < std::size_t(m_setup.plan.names.size()); i++)
      {
        const auto& r = results[i];
        if(!r || !r->ok)
          continue;
        const QRectF src = i < m_setup.sourceRects.size() ? m_setup.sourceRects[i]
                                                          : QRectF{0, 0, 1, 1};
        QString err;
        if(saveEquirectWarp(
               m_setup.plan.names[int(i)], equirectWarp(r->xyz, r->width, r->height, src),
               &err))
          written++;
        else
          m_log.push_back(QObject::tr("Warp map of %1: %2").arg(m_setup.plan.names[int(i)], err));
      }
      m_log.push_back(QObject::tr("Equirectangular warp maps written for %1 output(s)").arg(written));
    }
    if(!m_setup.blend)
    {
      m_status = QObject::tr("Done");
      m_state = State::Done;
      release();
      return;
    }
    std::vector<std::string> paths;
    for(const auto& n : m_setup.plan.names)
      paths.push_back(QString(n + QStringLiteral(".json")).toStdString());
    m_status = QObject::tr("Computing the blend maps");
    m_log.push_back(m_status);
    m_state = State::Blending;
    m_camera.reset();
    m_positions.reset();
    m_blend = std::async(std::launch::async, [paths] {
      std::vector<CalibrationFile> cals;
      std::string failed;
      for(const auto& p : paths)
      {
        auto c = loadCalibration(p);
        if(c.ok())
          cals.push_back(std::move(c));
        else
          failed += (failed.empty() ? "" : ", ") + c.error;
      }
      if(cals.empty())
      {
        ProjectorBlendMaps::Result r;
        r.status = "Failed: " + failed;
        return r;
      }
      ProjectorBlendMaps::Settings s;
      s.ramp = {0.f};
      s.exponent = {1.f};
      return ProjectorBlendMaps::compute(std::move(cals), s);
    });
    return;
  }
  if(!m_session.running())
    fail(QString::fromStdString(m_session.status()));
}
}
