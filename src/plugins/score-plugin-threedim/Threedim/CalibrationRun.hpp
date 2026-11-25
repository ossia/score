#pragma once
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Threedim/CalibrationPlan.hpp>
#include <Threedim/CalibrationPostProcess.hpp>
#include <Threedim/StructuredLightSession.hpp>

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace Threedim
{
//! A projector calibration of a multi-window device, outside the document:
//! the patterns go straight to the device's windows
//! (MultiWindowNode::setCalibration, one image per output), the camera is read
//! back through the document's GfxContext (Gfx::TextureCapture), and the
//! decoding, saving and maps (CalibrationPostProcess) run on the executor.
//! Nothing is added to the score and it does not need to play; nothing
//! blocks the thread that ticks it.
class CalibrationRun
{
  Q_DECLARE_TR_FUNCTIONS(Threedim::CalibrationRun)
public:
  using Image = score::gfx::MultiWindowNode::CalibrationImage;
  using Images = std::vector<std::shared_ptr<const Image>>;

  //! Where the camera frames come from: a Gfx::TextureCapture by default.
  class Camera
  {
  public:
    virtual ~Camera();
    virtual Gfx::TextureCapture::State state() const = 0;
    virtual QString reason() const = 0;
    virtual void setProducer(Gfx::port_index p) = 0;
    virtual void request() = 0;
    virtual bool take(std::vector<float>& rgba, int& width, int& height) = 0;
    virtual int64_t frames() const = 0;
  };

  //! Where the patterns go: the device's windows by default.
  struct Display
  {
    //! The windows of the device; negative once it is gone.
    std::function<int()> windows;
    //! Output i shows images[i], or its content when null.
    std::function<void(Images)> show;
    std::function<void()> clear;
  };
  static Display multiWindow(std::function<score::gfx::MultiWindowNode*()> node);

  struct Setup
  {
    QPointer<Gfx::GfxContext> gfx;
    Display display;
    //! The camera's texture producer, asked again on every tick: it moves when
    //! the device is created again; nullopt once it is gone.
    std::function<std::optional<Gfx::port_index>()> camera;
    //! A depth camera's positions, read with the camera when the session
    //! wants them; empty or nullopt for none.
    std::function<std::optional<Gfx::port_index>()> positions;
    //! Opens the camera on a producer; by default a Gfx::TextureCapture on gfx.
    std::function<std::unique_ptr<Camera>(Gfx::port_index)> openCamera;
    //! Asks the camera devices to stream with the score stopped (true), or
    //! releases that request to reopen them (false). The run does not release
    //! it when it ends: a camera reopened right after it was closed may not
    //! get its mode back, closing it for good is the caller's.
    std::function<void(bool)> keepStreaming;
    //! The frame size the camera device is set to: before the patterns, a
    //! camera that delivers another one is reopened, then refused.
    QSize expectedCameraSize;
    //! Distinct frames of the right size wanted before the patterns start.
    int warmupFrames{2};
    //! Without them for this long, the camera is reopened after each delay,
    //! then the run fails.
    double warmupMs{8000.};
    std::vector<double> reopenDelaysMs{3000., 8000.};
    //! Distinct camera frames after a pattern change before one is used: the
    //! capture and decoding pipeline of a webcam, and the windows, hold a few.
    int latencyFrames{3};

    std::function<void(float)> setExposure;
    std::function<void(bool)> setAutoExposure;
    //! Put back when the run ends, however it ends.
    std::optional<float> originalExposure;
    std::optional<bool> originalAutoExposure;

    //! Runs the session's decoding and the post-processing; by default on
    //! QThreadPool::globalInstance().
    StructuredLightSession::Executor executor;

    CalibrationPlan plan;
    //! Projector size and count come from the plan.
    StructuredLightSession::Settings settings;
    StructuredLightSession::Run run;
    //! Names, outputs and the viewer flag come from the plan and settings.
    CalibrationPostProcess::Options post;
  };

  enum class State
  {
    Idle,
    Opening,
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
  //! Redoes what follows a capture from the saved calibrations of the plan
  //! (from their measured maps when there are), without the patterns.
  bool recompute(double nowMs);
  void tick(double nowMs);
  void abort(const QString& why);

  State state() const noexcept { return m_state; }
  bool running() const noexcept
  {
    return m_state == State::Opening || m_state == State::Capturing
           || m_state == State::Blending;
  }
  QString status() const;
  float progress() const noexcept;
  //! Lines for the log since the last call.
  QStringList takeLog();
  const StructuredLightSession& session() const noexcept { return m_session; }
  const Setup& setup() const noexcept { return m_setup; }
  //! What the post-processing found, once Done.
  const std::optional<CalibrationPostProcess::Outcome>& outcome() const noexcept
  {
    return m_outcome;
  }

  struct Pending;

private:
  void openCamera(double now);
  void tickOpening(double now);
  void tickCapture(double now);
  bool readCamera(double now);
  void startSession(double now);
  void show();
  void apply(const StructuredLightSession::Events& ev);
  void launch(CalibrationPostProcess::Results results, bool fresh, bool saveOnly);
  void release();
  void fail(const QString& why);

  Setup m_setup;
  StructuredLightSession m_session;
  std::unique_ptr<Camera> m_camera, m_positions;
  std::vector<float> m_xyz;
  int m_xyzW{}, m_xyzH{};
  bool m_xyzRequested{};
  std::optional<Gfx::port_index> m_producer;
  bool m_streaming{};
  bool m_requested{};
  std::vector<float> m_rgba;
  int m_rgbaW{}, m_rgbaH{};
  uint64_t m_hash{};
  uint64_t m_distinct{};
  uint64_t m_distinctAtChange{};
  double m_openedAt{};
  double m_reopenAt{-1.};
  std::size_t m_reopens{};
  std::vector<int> m_shown;
  State m_state{State::Idle};
  QString m_status;
  QStringList m_log;
  std::shared_ptr<Pending> m_pending;
  bool m_saveOnly{};
  std::optional<CalibrationPostProcess::Outcome> m_outcome;
};
}
