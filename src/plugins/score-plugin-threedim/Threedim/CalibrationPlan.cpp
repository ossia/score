#include "CalibrationPlan.hpp"

#include <QRegularExpression>

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/StructuredLight.hpp>

#include <QCoreApplication>
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
QString tr(const char* s)
{
  return QCoreApplication::translate("Threedim::CalibrationPlan", s);
}

}

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, int minStripe, int phaseSteps)
{
  CalibrationPlan p;
  const auto& outs = ws.outputs;
  if(ws.mode != Gfx::WindowMode::MultiWindow || outs.empty())
  {
    p.warnings.push_back(tr("the device is not a multi-window device with outputs"));
    return p;
  }
  p.projectors = std::min<int>(int(outs.size()), 16);
  if(outs.size() > 16)
    p.warnings.push_back(
        tr("Structured Light Calibration takes 16 projectors: outputs 17 and "
                    "later are left out"));
  p.projectorSize = outs[0].windowSize;
  const QString setBase = Gfx::calibrationSetBase(base);
  for(int k = 0; k < p.projectors; k++)
  {
    const auto& o = outs[k];
    p.names.push_back(Gfx::calibrationSetName(setBase, k));
    if(o.windowSize != p.projectorSize && p.error.isEmpty())
      p.error = tr("output %1 is %2x%3 and output 1 %4x%5: the patterns of a run have one "
                   "size, calibrate them in separate runs (Outputs)")
                    .arg(k + 1)
                    .arg(o.windowSize.width())
                    .arg(o.windowSize.height())
                    .arg(p.projectorSize.width())
                    .arg(p.projectorSize.height());
    if(o.rotation != 0 || o.mirrorX || o.mirrorY)
      p.warnings.push_back(
          tr("output %1 is rotated or mirrored: the calibration is in the "
                      "rotated pixels")
              .arg(k + 1));
  }
  auto seq = sl::GraySequence::make(
      std::max(1, p.projectorSize.width()), std::max(1, p.projectorSize.height()),
      minStripe);
  if(phaseSteps > 0)
    seq.enablePhase(phaseSteps, 0, 2.2f);
  p.patternsPerProjector = seq.count();
  return p;
}

std::vector<int> parseOutputSelection(const QString& text, int count)
{
  std::vector<int> out;
  for(const auto& part : text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts))
  {
    const auto range = part.split('-');
    bool ok1 = false, ok2 = true;
    const qlonglong a = range[0].toLongLong(&ok1);
    const qlonglong b = range.size() > 1 ? range[1].toLongLong(&ok2) : a;
    if(!ok1 || !ok2 || range.size() > 2 || count <= 0)
      continue;
    const qlonglong lo = std::max<qlonglong>(1, std::min(a, b));
    const qlonglong hi = std::min<qlonglong>(count, std::max(a, b));
    for(qlonglong i = lo; i <= hi; i++)
      if(std::find(out.begin(), out.end(), int(i - 1)) == out.end())
        out.push_back(int(i - 1));
  }
  if(out.empty())
    for(int i = 0; i < count; i++)
      out.push_back(i);
  std::sort(out.begin(), out.end());
  return out;
}

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, const std::vector<int>& outputs,
    int minStripe, int phaseSteps)
{
  auto sub = ws;
  sub.outputs.clear();
  for(int i : outputs)
    if(i >= 0 && i < (int)ws.outputs.size())
      sub.outputs.push_back(ws.outputs[i]);
  auto p = planCalibration(sub, base, minStripe, phaseSteps);
  const QString setBase = Gfx::calibrationSetBase(base);
  p.outputs.clear();
  for(int k = 0; k < p.projectors && k < (int)outputs.size(); k++)
  {
    p.outputs.push_back(outputs[k]);
    if(k < p.names.size())
      p.names[k] = Gfx::calibrationSetName(setBase, outputs[k]);
  }
  static const QRegularExpression first{QStringLiteral("^output (\\d+)")};
  auto device = [&](QString w) {
    const auto m = first.match(w);
    if(!m.hasMatch())
      return w;
    const int k = m.captured(1).toInt() - 1;
    const int out = k >= 0 && k < (int)outputs.size() ? outputs[k] + 1 : k + 1;
    return w.replace(m.capturedStart(1), m.capturedLength(1), QString::number(out));
  };
  for(auto& w : p.warnings)
    w = device(w);
  if(!p.error.isEmpty())
  {
    static const QRegularExpression one{QStringLiteral("and output 1 ")};
    p.error = device(p.error).replace(
        one, QStringLiteral("and output %1 ").arg(outputs.empty() ? 1 : outputs[0] + 1));
  }
  return p;
}

Gfx::WindowSettings assignCalibrations(
    Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs, QStringList* notes)
{
  for(int k = 0; k < (int)ws.outputs.size() && k < names.size(); k++)
  {
    auto& o = ws.outputs[k];
    if(notes)
    {
      QStringList off;
      if(o.blendLeft.width > 0.f || o.blendRight.width > 0.f || o.blendTop.width > 0.f
         || o.blendBottom.width > 0.f)
        off.push_back(tr("edge blends"));
      if(!o.cornerWarp.isIdentity())
        off.push_back(tr("corner pin"));
      if(!off.isEmpty())
        notes->push_back(tr("output %1: %2 turned off, the calibration replaces them")
                             .arg(k + 1)
                             .arg(off.join(QStringLiteral(", "))));
    }
    o.blendLeft = o.blendRight = o.blendTop = o.blendBottom = {};
    o.cornerWarp = {};
    o.maps = {};
    Gfx::applyCalibration(o.maps, names[k] + QStringLiteral(".json"));
    for(const auto& slot : Gfx::calibrationFileSlots())
    {
      QString& f = o.maps.*slot.member;
      if(const QFileInfo fi{f};
         !f.isEmpty() && fi.lastModified().toMSecsSinceEpoch() + 1000 < sinceMs)
        f.clear();
    }
    o.color.linearBlend = true;
  }
  return ws;
}

Gfx::WindowSettings assignCalibrations(
    Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs,
    const std::vector<int>& outputs, QStringList* notes)
{
  for(int k = 0; k < (int)outputs.size() && k < names.size(); k++)
  {
    const int i = outputs[k];
    if(i < 0 || i >= (int)ws.outputs.size())
      continue;
    Gfx::WindowSettings one;
    one.outputs = {ws.outputs[i]};
    QStringList n;
    one = assignCalibrations(std::move(one), QStringList{names[k]}, sinceMs, notes ? &n : nullptr);
    for(auto& line : n)
      notes->push_back(line.replace(QStringLiteral("output 1:"), QStringLiteral("output %1:").arg(i + 1)));
    ws.outputs[i] = one.outputs[0];
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

double estimatePostMs(const CalibrationPlan& plan, const PostTiming& t)
{
  const double scale = double(plan.projectorSize.width()) * plan.projectorSize.height()
                       / (3840. * 2160.) * std::max(0, plan.projectors);
  double ms = 0.;
  if(t.blend)
    ms += (t.blackCompensation ? t.blackMs : t.blendMs) * scale;
  if(t.completePlanes)
    ms += t.planesMs * scale;
  return ms;
}

CalibrationReport readCalibrationResults(
    const QStringList& names, qint64 sinceMs, const std::vector<int>& outputs)
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
    if(o["mode"].toString() == QStringLiteral("viewer"))
      lines.push_back(tr("output %1: %2, coverage %3%")
                          .arg((k < (int)outputs.size() ? outputs[k] : k) + 1)
                          .arg(fi.fileName())
                          .arg(r.coverage.back(), 0, 'f', 1));
    else
      lines.push_back(tr("output %1: %2, RMS %3 px, coverage %4%")
                          .arg((k < (int)outputs.size() ? outputs[k] : k) + 1)
                          .arg(fi.fileName())
                          .arg(r.rms.back(), 0, 'f', 2)
                          .arg(r.coverage.back(), 0, 'f', 1));
  }
  r.text = lines.join('\n');
  return r;
}

}
