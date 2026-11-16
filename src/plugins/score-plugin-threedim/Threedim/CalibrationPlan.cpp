#include "CalibrationPlan.hpp"

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/StructuredLight.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace Threedim
{
namespace
{
QString rectString(const QRectF& r)
{
  return QStringLiteral("%1 %2 %3 %4")
      .arg(r.x(), 0, 'g', 6)
      .arg(r.y(), 0, 'g', 6)
      .arg(r.width(), 0, 'g', 6)
      .arg(r.height(), 0, 'g', 6);
}
}

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, int minStripe, int phaseSteps)
{
  CalibrationPlan p;
  const auto& outs = ws.outputs;
  if(ws.mode != Gfx::WindowMode::MultiWindow || outs.empty())
  {
    p.warnings.push_back(QObject::tr("the device is not a multi-window device with outputs"));
    return p;
  }
  p.projectors = std::min<int>(int(outs.size()), 16);
  if(outs.size() > 16)
    p.warnings.push_back(
        QObject::tr("Structured Light Calibration takes 16 projectors: outputs 17 and "
                    "later are left out"));
  p.projectorSize = outs[0].windowSize;
  p.canvasSize = QSize(std::max(1, ws.inputWidth), std::max(1, ws.inputHeight));
  QStringList rects;
  const QString setBase = Gfx::calibrationSetBase(base);
  for(int k = 0; k < p.projectors; k++)
  {
    const auto& o = outs[k];
    rects.push_back(rectString(o.sourceRect));
    p.names.push_back(Gfx::calibrationSetName(setBase, k));
    if(o.windowSize != p.projectorSize)
      p.warnings.push_back(
          QObject::tr("output %1 is %2x%3, output 1 %4x%5: the patterns use one size")
              .arg(k + 1)
              .arg(o.windowSize.width())
              .arg(o.windowSize.height())
              .arg(p.projectorSize.width())
              .arg(p.projectorSize.height()));
    const int sw = int(std::lround(o.sourceRect.width() * p.canvasSize.width()));
    const int sh = int(std::lround(o.sourceRect.height() * p.canvasSize.height()));
    if(std::abs(sw - o.windowSize.width()) > 1 || std::abs(sh - o.windowSize.height()) > 1)
      p.warnings.push_back(
          QObject::tr("output %1 shows %2x%3 canvas pixels on a %4x%5 window: the "
                      "patterns are resampled; make the source rect match the window")
              .arg(k + 1)
              .arg(sw)
              .arg(sh)
              .arg(o.windowSize.width())
              .arg(o.windowSize.height()));
    if(o.rotation != 0 || o.mirrorX || o.mirrorY)
      p.warnings.push_back(
          QObject::tr("output %1 is rotated or mirrored: the calibration is in the "
                      "rotated pixels")
              .arg(k + 1));
  }
  p.canvasLayout = rects.join(QStringLiteral("; "));
  auto seq = sl::GraySequence::make(
      std::max(1, p.projectorSize.width()), std::max(1, p.projectorSize.height()),
      minStripe);
  if(phaseSteps > 0)
    seq.enablePhase(phaseSteps, 0, 2.2f);
  p.patternsPerProjector = seq.count();
  return p;
}

Gfx::WindowSettings neutralOutputs(Gfx::WindowSettings ws)
{
  for(auto& o : ws.outputs)
  {
    o.blendLeft = o.blendRight = o.blendTop = o.blendBottom = {};
    o.cornerWarp = {};
    o.maps = {};
    o.color = {};
  }
  return ws;
}

Gfx::WindowSettings
assignCalibrations(Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs)
{
  ws = neutralOutputs(std::move(ws));
  for(int k = 0; k < (int)ws.outputs.size() && k < names.size(); k++)
  {
    auto& maps = ws.outputs[k].maps;
    Gfx::applyCalibration(maps, names[k] + QStringLiteral(".json"));
    for(const auto& slot : Gfx::calibrationFileSlots())
    {
      QString& f = maps.*slot.member;
      if(const QFileInfo fi{f};
         !f.isEmpty() && fi.lastModified().toMSecsSinceEpoch() + 1000 < sinceMs)
        f.clear();
    }
    ws.outputs[k].color.linearBlend = true;
  }
  return ws;
}

double estimateCaptureMs(const CalibrationPlan& plan, const CaptureTiming& t)
{
  const double frame = t.settleMs + std::max(1, t.framesToAverage) * t.cameraFrameMs;
  double ms = 0.;
  for(int k = 0; k < plan.projectors; k++)
  {
    const int extra = k < int(t.extraFrames.size()) ? t.extraFrames[k] : 0;
    const int changes = k < int(t.exposureChanges.size()) ? t.exposureChanges[k] : 0;
    ms += (plan.patternsPerProjector + extra) * frame
          + changes * std::max(0., t.exposureSettleMs - t.settleMs);
  }
  return ms;
}

bool CalibrationSession::begin(const QString& device, const Gfx::WindowSettings& current)
{
  if(m_original)
    return false;
  if(!m_apply || !m_apply(device, neutralOutputs(current)))
    return false;
  m_device = device;
  m_original = current;
  return true;
}

bool CalibrationSession::restore()
{
  if(!m_original)
    return true;
  const bool ok = m_apply && m_apply(m_device, *m_original);
  m_original.reset();
  return ok;
}

bool CalibrationSession::finish(const QStringList& names, qint64 sinceMs)
{
  if(!m_original)
    return false;
  const bool ok = m_apply && m_apply(m_device, assignCalibrations(*m_original, names, sinceMs));
  m_original.reset();
  return ok;
}

void CalibrationSession::drop() noexcept
{
  m_original.reset();
}

CalibrationReport readCalibrationResults(const QStringList& names, qint64 sinceMs)
{
  CalibrationReport r;
  QStringList lines;
  for(int k = 0; k < names.size(); k++)
  {
    const QFileInfo fi{names[k] + QStringLiteral(".json")};
    if(!fi.isFile() || fi.lastModified().toMSecsSinceEpoch() + 1000 < sinceMs)
    {
      r.rms.push_back(-1.);
      r.coverage.push_back(-1.);
      continue;
    }
    QFile f{fi.filePath()};
    QJsonParseError err{};
    const auto doc = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll(), &err)
                                                 : QJsonDocument{};
    if(err.error != QJsonParseError::NoError || !doc.isObject())
    {
      r.rms.push_back(-1.);
      r.coverage.push_back(-1.);
      continue;
    }
    const auto o = doc.object();
    r.saved++;
    r.rms.push_back(o["rms"].toDouble());
    r.coverage.push_back(o["coverage"].toDouble());
    lines.push_back(QObject::tr("output %1: %2, RMS %3 px, coverage %4%")
                        .arg(k + 1)
                        .arg(fi.fileName())
                        .arg(r.rms.back(), 0, 'f', 2)
                        .arg(r.coverage.back(), 0, 'f', 1));
  }
  r.text = lines.join('\n');
  return r;
}

}
