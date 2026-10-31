#include "CalibrationPlan.hpp"

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/StructuredLight.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

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
  p.projectors = std::min<int>(int(outs.size()), 4);
  if(outs.size() > 4)
    p.warnings.push_back(
        QObject::tr("Structured Light Calibration takes 4 projectors: outputs 5 and "
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

Gfx::WindowSettings assignCalibrations(Gfx::WindowSettings ws, const QStringList& names)
{
  for(int k = 0; k < (int)ws.outputs.size() && k < names.size(); k++)
  {
    Gfx::applyCalibration(ws.outputs[k].maps, names[k] + QStringLiteral(".json"));
    ws.outputs[k].color.linearBlend = true;
  }
  return ws;
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
    if(!f.open(QIODevice::ReadOnly))
      continue;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
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
