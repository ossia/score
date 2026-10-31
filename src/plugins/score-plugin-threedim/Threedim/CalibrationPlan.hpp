#pragma once
#include <Gfx/WindowDevice.hpp>

#include <QSize>
#include <QStringList>

#include <vector>

namespace Threedim
{
//! What a Structured Light Calibration run needs to cover a multi-window device.
struct CalibrationPlan
{
  int projectors{};
  QSize projectorSize;
  QSize canvasSize;
  //! "x y w h" per output, normalised, from the source rects.
  QString canvasLayout;
  //! Calibration base per output: base, base_p1, ...
  QStringList names;
  int patternsPerProjector{};
  QStringList warnings;
};

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, int minStripe = 2,
    int phaseSteps = 4);

//! The outputs with their warp, blend, maps and colour off, so that the
//! patterns reach the projector pixels as they are.
Gfx::WindowSettings neutralOutputs(Gfx::WindowSettings ws);

//! `ws` with output k following calibration names[k], blended in linear light.
Gfx::WindowSettings assignCalibrations(Gfx::WindowSettings ws, const QStringList& names);

struct CalibrationReport
{
  int saved{};
  std::vector<double> rms, coverage;
  QString text;
};
//! Reads the name.json written since `sinceMs` (ms since epoch) for each name.
CalibrationReport readCalibrationResults(const QStringList& names, qint64 sinceMs);

}
