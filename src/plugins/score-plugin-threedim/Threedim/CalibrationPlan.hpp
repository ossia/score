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
  QSize canvasSize;
  //! "x y w h" per output, normalised, from the source rects.
  QString canvasLayout;
  //! Calibration base per output: base, base_p1, ...
  QStringList names;
  int patternsPerProjector{};
  QStringList warnings;
  //! The device output each projector of the plan is (names[k] belongs to
  //! outputs[k]); empty means outputs 0, 1, ...
  std::vector<int> outputs;
};

//! "1-4, 6" (1 based) -> {0, 1, 2, 3, 5}; empty -> every output. Indices past
//! `count` are dropped.
std::vector<int> parseOutputSelection(const QString& text, int count);

//! The plan of the selected outputs only, named after their output index
//! (base, base_p1, ... of the whole device).
CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, const std::vector<int>& outputs,
    int minStripe, int phaseSteps);

CalibrationPlan planCalibration(
    const Gfx::WindowSettings& ws, const QString& base, int minStripe = 2,
    int phaseSteps = 4);

//! The outputs with their warp, blend, maps and colour off, so that the
//! patterns reach the projector pixels as they are.
Gfx::WindowSettings neutralOutputs(Gfx::WindowSettings ws);

//! The neutral outputs of `ws` with output k following calibration names[k],
//! blended in linear light. Files of a calibration older than `sinceMs` (ms
//! since epoch) are left out.
Gfx::WindowSettings
assignCalibrations(Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs = 0);
//! Output outputs[k] follows names[k]; the other outputs are left as they are.
Gfx::WindowSettings assignCalibrations(
    Gfx::WindowSettings ws, const QStringList& names, qint64 sinceMs,
    const std::vector<int>& outputs);

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

//! A device whose output stage is off for a calibration: keeps its settings
//! from the first begin() until restore() or finish().
class CalibrationSession
{
public:
  using Apply = std::function<bool(const QString& device, const Gfx::WindowSettings&)>;
  explicit CalibrationSession(Apply apply)
      : m_apply{std::move(apply)}
  {
  }

  bool active() const noexcept { return m_original.has_value(); }
  const QString& device() const noexcept { return m_device; }
  const std::optional<Gfx::WindowSettings>& original() const noexcept { return m_original; }

  //! Turns the output stage of `device` off; false when a session is active.
  bool begin(const QString& device, const Gfx::WindowSettings& current);
  //! Puts the device back as it was; false when it could not be updated.
  bool restore();
  //! Assigns the calibrations over the device's neutral settings.
  bool finish(const QStringList& names, qint64 sinceMs);
  //! Forgets the session without touching the device.
  void drop() noexcept;

private:
  Apply m_apply;
  QString m_device;
  std::optional<Gfx::WindowSettings> m_original;
};

struct CalibrationReport
{
  int saved{};
  std::vector<double> rms, coverage;
  QString text;
};
//! Reads the name.json written since `sinceMs` (ms since epoch) for each name.
CalibrationReport readCalibrationResults(const QStringList& names, qint64 sinceMs);

}
