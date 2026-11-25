#pragma once
#include <Gfx/WindowDevice.hpp>

#include <QSize>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

namespace Threedim
{
//! What a Structured Light Calibration run needs to cover a multi-window device.
struct CalibrationPlan
{
  int projectors{};
  QSize projectorSize;
  //! Calibration base per output: base, base_p1, ...
  QStringList names;
  int patternsPerProjector{};
  QStringList warnings;
  //! Why the plan cannot run (outputs of different sizes); empty when it can.
  QString error;
  //! The device output each projector of the plan is (names[k] belongs to
  //! outputs[k]); empty means outputs 0, 1, ...
  std::vector<int> outputs;
};

//! "1-4, 6" (1 based) -> {0, 1, 2, 3, 5}; empty -> every output. Ranges are
//! clipped to 1..count.
std::vector<int> parseOutputSelection(const QString& text, int count);

//! The plan of the selected outputs only, named after their output index
//! (base, base_p1, ... of the whole device).
CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, const std::vector<int>& outputs,
    int minStripe, int phaseSteps);

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, int minStripe = 2,
    int phaseSteps = 4);

//! Output k of `ws` following calibration names[k], its edge blends and
//! corner pin (made for the content without a calibration) off, blended in
//! linear light; its colour gain, offset and gamma are kept. Files of a
//! calibration older than `sinceMs` (ms since epoch) are left out. `notes`
//! says what was turned off.
Gfx::WindowSettings assignCalibrations(
    Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs,
    QStringList* notes = nullptr);
//! Output outputs[k] follows names[k]; the other outputs are left as they are.
Gfx::WindowSettings assignCalibrations(
    Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs,
    const std::vector<int>& outputs, QStringList* notes = nullptr);

//! What a capture of the plan takes, for each projector: the frames after the
//! Gray code and the camera exposure changes among them.
struct CaptureTiming
{
  double settleMs{};
  double exposureSettleMs{};
  int framesToAverage{1};
  double cameraFrameMs{1000. / 30.};
  std::vector<int> extraFrames;
  std::vector<int> exposureChanges;
};
double estimateCaptureMs(const CalibrationPlan& plan, const CaptureTiming& t);

//! What the post-processing of the plan takes, measured at 3840x2160 per
//! output: the blend (much longer when the black level is evened out) and the
//! plane completion of a viewer camera's maps.
struct PostTiming
{
  bool blend{};
  bool blackCompensation{};
  bool completePlanes{};
  double blendMs{1700.}, blackMs{12000.}, planesMs{2000.};
};
double estimatePostMs(const CalibrationPlan& plan, const PostTiming& t);

struct CalibrationReport
{
  int saved{};
  std::vector<double> rms, coverage;
  QString text;
};
//! Reads the name.json written since `sinceMs` (ms since epoch) for each name.
CalibrationReport readCalibrationResults(
    const QStringList& names, qint64 sinceMs, const std::vector<int>& outputs = {});

}
