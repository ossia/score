#pragma once
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Threedim/CalibrationPlan.hpp>
#include <Threedim/ProjectorBlend.hpp>
#include <Threedim/StructuredLightSession.hpp>

#include <QString>
#include <QStringList>

#include <functional>
#include <future>
#include <memory>
#include <optional>

namespace Threedim
{
//! A projector calibration of a multi-window device, outside the document:
//! the patterns go straight to the device's windows
//! (MultiWindowNode::setCalibration), the camera is read back through the
//! document's GfxContext (Gfx::TextureCapture), and Projector Blend Maps runs
//! on the results. Nothing is added to the score and it does not need to play.
class CalibrationRun
{
public:
  struct Setup
  {
    Gfx::GfxContext* gfx{};
    //! The device's node, looked up again at every tick: null once it is gone.
    std::function<score::gfx::MultiWindowNode*()> node;
    Gfx::port_index camera{-1, -1};
    Gfx::port_index positions{-1, -1};
    //! Opens (true) or closes (false) the camera devices when the score does
    //! not play: their capture only runs during execution otherwise. The run
    //! opens them at its start and closes them only to reopen them; closing
    //! them for good is the caller's (a camera reopened right after it was
    //! closed may not get its mode back).
    std::function<void(bool)> cameraActive;
    //! The frame size the camera device is set to; a camera delivering another
    //! one did not open in its mode (seen on Windows when reopened right after
    //! a close): it is closed, reopened after a pause, and the run fails if it
    //! still does not.
    QSize expectedCameraSize;
    //! Even out the black level (beta maps raised to the brightest black);
    //! off, the blend only writes the alpha maps' weights.
    bool blackCompensation{};
    //! Emissive screens: written into name.json ("display": "screen") so that
    //! a later projector run leaves them out of its blend.
    bool screens{};
    std::function<void(float)> setExposure;
    std::function<void(bool)> setAutoExposure;
    //! Put back on setExposure when the run ends, however it ends.
    std::optional<float> originalExposure;
    CalibrationPlan plan;
    //! Projector size, count, save path and auto save come from the plan.
    StructuredLightSession::Settings settings;
    bool blend{true};
    //! Viewer camera on flat walls and floor: rebuild each direction map from
    //! its planes (PlaneCompletion), filling what the camera did not see.
    bool completePlanes{};
    //! The calibrations Projector Blend Maps runs on: those of this run, and
    //! of the outputs it does not measure; empty = this run's.
    QStringList blendCalibrations;
    //! Viewer camera: where the surface points come from, for a viewpoint that
    //! can move afterwards (MultiWindowNode points maps).
    enum class Geometry
    {
      Directions,
      Box,
      Model
    } geometry{Geometry::Directions};
    //! Front, back, left, right, floor, ceiling distances from the camera (m).
    std::array<float, 6> box{};
    QString model;
    //! The camera in the room's frame: position, and Euler degrees.
    std::array<float, 3> cameraPosition{};
    std::array<float, 3> cameraRotation{};
    double planeThresholdArcmin{6.};
    //! Camera frames read back after a pattern change before one is used: the
    //! capture and decoding pipeline of a webcam holds a few frames.
    int latencyFrames{3};
    //! No new camera frame for this long while capturing fails the run.
    double stallMs{8000.};
    //! The device's canvas and the outputs' source rects: a viewer-camera run
    //! on an equirectangular canvas also writes each output's warp map.
    Gfx::CanvasLayout canvas;
    std::vector<QRectF> sourceRects;
  };

  enum class State
  {
    Idle,
    Capturing,
    Blending,
    Done,
    Failed
  };

  explicit CalibrationRun(Setup s);
  ~CalibrationRun();
  CalibrationRun(const CalibrationRun&) = delete;
  CalibrationRun& operator=(const CalibrationRun&) = delete;

  bool start(double nowMs);
  //! Redoes what follows a capture (plane completion, room cast, warp, points
  //! and blend maps) from the saved calibrations of the plan, from their
  //! measured maps when there are: for another room, camera pose or option,
  //! without projecting the patterns again.
  bool recompute(double nowMs);
  void tick(double nowMs);
  void abort(const QString& why);

  State state() const noexcept { return m_state; }
  //! Whether the calibration name.json was made for emissive screens.
  static bool isScreen(const QString& json);
  static void markDisplay(const QString& json, bool screens);

  bool running() const noexcept
  {
    return m_state == State::Capturing || m_state == State::Blending;
  }
  QString status() const;
  float progress() const noexcept;
  //! Lines for the log since the last call.
  QStringList takeLog();
  const StructuredLightSession& session() const noexcept { return m_session; }
  const Setup& setup() const noexcept { return m_setup; }

private:
  void show(score::gfx::MultiWindowNode& node);
  void release();
  void fail(const QString& why);
  void pullCamera();
  void launchMaps(
      std::vector<std::shared_ptr<const StructuredLightCore::Result>> results, bool fresh);
  //! Writes each decoded result under its output's name.
  void saveResults();
  bool m_cameraOpened{};
  //! Distinct camera images seen: a readback of the frame already read back
  //! (the hub renders faster than a webcam delivers) does not count.
  int64_t m_cameraFrames{};
  uint64_t m_lastHash{};
  int64_t m_framesAtChange{};
  double m_lastFrameAt{};
  bool m_frameSizeLogged{};
  int m_reopens{};
  double m_reopenAt{-1.};
  QString outputLabel(int k) const;

  Setup m_setup;
  StructuredLightSession m_session;
  std::unique_ptr<Gfx::TextureCapture> m_camera, m_positions;
  std::vector<float> m_rgba, m_xyz;
  int m_rgbaW{}, m_rgbaH{}, m_xyzW{}, m_xyzH{};
  int m_shownVersion{-1};
  State m_state{State::Idle};
  QString m_status;
  QStringList m_log;
  struct Finish
  {
    QStringList log;
    QString status;
  };
  std::future<Finish> m_blend;
};
}
