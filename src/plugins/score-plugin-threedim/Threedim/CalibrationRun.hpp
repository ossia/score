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
    //! not play: their capture only runs during execution otherwise.
    std::function<void(bool)> cameraActive;
    std::function<void(float)> setExposure;
    std::function<void(bool)> setAutoExposure;
    CalibrationPlan plan;
    //! Projector size, count, save path and auto save come from the plan.
    StructuredLightSession::Settings settings;
    bool blend{true};
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
  void tick(double nowMs);
  void abort(const QString& why);

  State state() const noexcept { return m_state; }
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
  bool m_cameraOpened{};
  //! Distinct camera images seen: a readback of the frame already read back
  //! (the hub renders faster than a webcam delivers) does not count.
  int64_t m_cameraFrames{};
  uint64_t m_lastHash{};
  int64_t m_framesAtChange{};
  double m_lastFrameAt{};
  bool m_frameSizeLogged{};

  Setup m_setup;
  StructuredLightSession m_session;
  std::unique_ptr<Gfx::TextureCapture> m_camera, m_positions;
  std::vector<float> m_rgba, m_xyz;
  int m_rgbaW{}, m_rgbaH{}, m_xyzW{}, m_xyzH{};
  int m_shownVersion{-1};
  State m_state{State::Idle};
  QString m_status;
  QStringList m_log;
  std::future<ProjectorBlendMaps::Result> m_blend;
};
}
