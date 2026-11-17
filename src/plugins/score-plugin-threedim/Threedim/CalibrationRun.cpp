#include "CalibrationRun.hpp"

#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/EquirectWarp.hpp>
#include <Threedim/PlaneCompletion.hpp>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFileInfo>
#include <QObject>

#include <chrono>
#include <cstring>
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
  st.autoSave = false;
}

CalibrationRun::~CalibrationRun()
{
  m_session.cancelComputation();
  release();
}

void CalibrationRun::pullCamera()
{
  for(const auto& p : {m_setup.camera, m_setup.positions})
    if(p.node >= 0 && m_setup.gfx)
    {
      score::gfx::Message m;
      m.node_id = p.node;
      m_setup.gfx->send_message(std::move(m));
    }
}

void CalibrationRun::markDisplay(const QString& json, bool screens)
{
  QFile f{json};
  if(!f.open(QIODevice::ReadOnly))
    return;
  auto o = QJsonDocument::fromJson(f.readAll()).object();
  f.close();
  o.insert(QStringLiteral("display"), screens ? QStringLiteral("screen") : QStringLiteral("projector"));
  QSaveFile out{json};
  if(out.open(QIODevice::WriteOnly))
  {
    out.write(QJsonDocument{o}.toJson());
    out.commit();
  }
}

bool CalibrationRun::isScreen(const QString& json)
{
  QFile f{json};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("display"))
         == QStringLiteral("screen");
}

QString CalibrationRun::outputLabel(int k) const
{
  const auto& sel = m_setup.plan.outputs;
  return QString::number((k < (int)sel.size() ? sel[k] : k) + 1);
}

void CalibrationRun::saveResults()
{
  const auto& results = m_session.results();
  for(int i = 0; i < (int)results.size() && i < m_setup.plan.names.size(); i++)
    if(const auto& r = results[i]; r && r->ok && !r->fromFile)
    {
      const QString json = m_setup.plan.names[i] + QStringLiteral(".json");
      if(!StructuredLightCore::saveFile(json.toStdString(), *r))
      {
        m_log.push_back(QObject::tr("Output %1: cannot write %2")
                            .arg(outputLabel(i))
                            .arg(json));
        continue;
      }
      markDisplay(json, m_setup.screens);
    }
}

void CalibrationRun::release()
{
  m_camera.reset();
  m_positions.reset();
  m_cameraOpened = false;
  if(m_setup.originalExposure && m_setup.setExposure)
    m_setup.setExposure(*std::exchange(m_setup.originalExposure, std::nullopt));
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

  if(m_setup.cameraActive)
  {
    m_setup.cameraActive(true);
    m_cameraOpened = true;
  }
  pullCamera();
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
  m_cameraFrames = 0;
  m_lastHash = 0;
  m_lastFrameAt = now;
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
  m_framesAtChange = m_cameraFrames;
  auto img = std::make_shared<score::gfx::MultiWindowNode::CalibrationImage>();
  img->width = d.width;
  img->height = d.height;
  img->rgba = d.rgba;
  const int windows = int(node.windowOutputs().size());
  std::vector<std::shared_ptr<const score::gfx::MultiWindowNode::CalibrationImage>> images(
      windows);
  const auto& sel = m_setup.plan.outputs;
  for(int k = 0; k < m_setup.plan.projectors; k++)
  {
    const int i = k < (int)sel.size() ? sel[k] : k;
    if(i >= 0 && i < windows && (d.idle || k == d.projector))
      images[i] = img;
  }
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
      auto r = m_blend.get();
      m_log.append(r.log);
      m_status = r.status;
      m_state = State::Done;
      release();
    }
    return;
  }

  if(m_reopenAt >= 0.)
  {
    if(now < m_reopenAt)
      return;
    m_reopenAt = -1.;
    if(m_setup.cameraActive)
      m_setup.cameraActive(true);
    m_camera = std::make_unique<Gfx::TextureCapture>(*m_setup.gfx, m_setup.camera);
    m_rgba.clear();
    m_lastFrameAt = now;
  }
  pullCamera();
  if(m_camera)
  {
    m_camera->take(m_rgba, m_rgbaW, m_rgbaH);
    uint64_t h = 1469598103934665603ull;
    for(std::size_t i = 0; i < m_rgba.size(); i += 97)
    {
      uint32_t b;
      std::memcpy(&b, &m_rgba[i], 4);
      h = (h ^ b) * 1099511628211ull;
    }
    if(!m_rgba.empty() && h != m_lastHash)
    {
      m_lastHash = h;
      m_cameraFrames++;
      m_lastFrameAt = now;
    }
    else if(m_session.capturing() && now - m_lastFrameAt > m_setup.stallMs)
    {
      if(m_reopens++ < 2 && m_setup.cameraActive)
      {
        m_log.push_back(QObject::tr("The camera delivered no frame for %1 s, reopening it")
                            .arg(int(m_setup.stallMs / 1000.)));
        m_camera.reset();
        m_rgba.clear();
        m_frameSizeLogged = false;
        m_setup.cameraActive(false);
        m_reopenAt = now + 2000.;
        return;
      }
      abort(QObject::tr("The camera delivered no frame for %1 s: is it streaming, or held "
                        "by another program?")
                .arg(int(m_setup.stallMs / 1000.)));
      return;
    }
    if(!m_frameSizeLogged && !m_rgba.empty() && m_cameraFrames >= 2)
    {
      const QSize want = m_setup.expectedCameraSize;
      if(want.isValid() && !want.isEmpty() && QSize(m_rgbaW, m_rgbaH) != want)
      {
        if(m_reopens++ < 2 && m_setup.cameraActive)
        {
          m_log.push_back(QObject::tr("Camera: %1x%2 instead of %3x%4, reopening it")
                              .arg(m_rgbaW)
                              .arg(m_rgbaH)
                              .arg(want.width())
                              .arg(want.height()));
          m_camera.reset();
          m_rgba.clear();
          m_setup.cameraActive(false);
          m_reopenAt = now + 2000.;
          return;
        }
        abort(QObject::tr("The camera delivers %1x%2 and its device is set to %3x%4: it did "
                          "not open in its mode. Close what else uses it, or reconnect it.")
                  .arg(m_rgbaW)
                  .arg(m_rgbaH)
                  .arg(want.width())
                  .arg(want.height()));
        return;
      }
      m_frameSizeLogged = true;
      m_log.push_back(QObject::tr("Camera: %1x%2").arg(m_rgbaW).arg(m_rgbaH));
    }
  }
  if(m_positions)
    m_positions->take(m_xyz, m_xyzW, m_xyzH);
  const bool fresh = !m_camera || m_cameraFrames >= m_framesAtChange + m_setup.latencyFrames;
  StructuredLightSession::Frame f;
  if(m_session.wantsCamera() && !m_rgba.empty() && fresh)
  {
    f.rgba = m_rgba.data();
    f.width = m_rgbaW;
    f.height = m_rgbaH;
  }
  if(m_session.wantsPositions() && !m_xyz.empty() && fresh)
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
    saveResults();
    m_status = m_setup.blend ? QObject::tr("Computing the warp and blend maps")
                             : QObject::tr("Computing the maps");
    m_log.push_back(m_status);
    m_state = State::Blending;
    m_camera.reset();
    m_positions.reset();
    m_blend = std::async(
        std::launch::async,
        [results = m_session.results(), setup = m_setup]() -> Finish {
      Finish out;
      const auto label = [&](int k) {
        const auto& sel = setup.plan.outputs;
        return QString::number((k < (int)sel.size() ? sel[k] : k) + 1);
      };
      const bool viewer = setup.settings.mode == StructuredLightSession::ViewerCamera;
      const int n = std::min<int>(int(results.size()), int(setup.plan.names.size()));
      for(int i = 0; i < n; i++)
      {
        const auto& r = results[i];
        if(!r || !r->ok)
          continue;
        const QString base = setup.plan.names[i];
        std::vector<float> directions = r->xyz;
        if(viewer && setup.completePlanes)
        {
          const auto c = completeFromPlanes(
              r->xyz, r->width, r->height, setup.planeThresholdArcmin, 2);
          if(c.ok)
          {
            QStringList residuals;
            for(std::size_t k = 0; k < c.medianArcmin.size(); k++)
              residuals.push_back(QStringLiteral("%1'").arg(c.medianArcmin[k], 0, 'f', 1));
            out.log.push_back(QObject::tr("Output %1: %2 plane(s), median residual %3, %4% "
                                          "of the measured pixels explained")
                                  .arg(label(i))
                                  .arg(int(c.homographies.size()))
                                  .arg(residuals.join(QStringLiteral(" / ")))
                                  .arg(c.explained * 100., 0, 'f', 1));
            for(const char* ext : {".xyz.pfm", ".valid.pfm"})
            {
              const QString from = base + QString::fromLatin1(ext);
              const QString to = base + QStringLiteral(".measured") + QString::fromLatin1(ext);
              QFile::remove(to);
              QFile::copy(from, to);
            }
            auto completed = *r;
            completed.xyz = c.directions;
            completed.coverage = 100.f;
            completed.status = r->status + " | completed from " + c.status;
            if(StructuredLightCore::saveFile(
                   QString(base + QStringLiteral(".json")).toStdString(), completed))
            {
              markDisplay(base + QStringLiteral(".json"), setup.screens);
              directions = std::move(completed.xyz);
            }
            else
              out.log.push_back(QObject::tr("Output %1: cannot write the completed map").arg(label(i)));
          }
          else
            out.log.push_back(QObject::tr("Output %1: not completed from planes (%2)")
                                  .arg(label(i))
                                  .arg(QString::fromStdString(c.status)));
        }
        if(viewer && setup.canvas.projection == Gfx::CanvasProjection::Equirectangular)
        {
          const QRectF src
              = i < int(setup.sourceRects.size()) ? setup.sourceRects[i] : QRectF{0, 0, 1, 1};
          QString err;
          if(!saveEquirectWarp(base, equirectWarp(directions, r->width, r->height, src), &err))
            out.log.push_back(QObject::tr("Output %1: warp map not written (%2)").arg(label(i)).arg(err));
        }
      }
      if(viewer && setup.canvas.projection == Gfx::CanvasProjection::Equirectangular)
        out.log.push_back(QObject::tr("Equirectangular warp maps written"));

      if(!setup.blend)
      {
        out.status = QObject::tr("Done");
        return out;
      }
      std::vector<CalibrationFile> cals;
      std::string failed;
      QStringList blendSet = setup.blendCalibrations;
      if(blendSet.isEmpty())
        for(int i = 0; i < n; i++)
          blendSet.push_back(setup.plan.names[i] + QStringLiteral(".json"));
      for(const auto& path : blendSet)
      {
        auto c = loadCalibration(path.toStdString());
        if(c.ok())
          cals.push_back(std::move(c));
        else
          failed += (failed.empty() ? "" : ", ") + c.error;
      }
      if(cals.empty())
      {
        out.log.push_back(QObject::tr("Projector Blend Maps: Failed: %1").arg(QString::fromStdString(failed)));
        out.status = QObject::tr("Done");
        return out;
      }
      ProjectorBlendMaps::Settings bs;
      bs.ramp = {0.f};
      bs.exponent = {1.f};
      bs.blackCompensation = setup.blackCompensation;
      const auto b = ProjectorBlendMaps::compute(std::move(cals), bs);
      out.log.push_back(QObject::tr("Projector Blend Maps: %1").arg(QString::fromStdString(b.status)));
      out.status = QObject::tr("Done");
      return out;
    });
    return;
  }
  if(!m_session.running())
  {
    const auto& results = m_session.results();
    if(results.empty())
      fail(QString::fromStdString(m_session.status()));
    else
    {
      saveResults();
      int ok = 0;
      for(std::size_t i = 0; i < results.size(); i++)
      {
        const auto& r = results[i];
        ok += r && r->ok;
        m_log.push_back(QObject::tr("Output %1: %2")
                            .arg(outputLabel(int(i)))
                            .arg(r ? QString::fromStdString(r->status) : QString{}));
      }
      fail(QObject::tr("%1 of %2 outputs calibrated; the others failed, nothing was "
                       "assigned (the calibrated ones are saved)")
               .arg(ok)
               .arg(int(results.size())));
    }
  }
}
}
