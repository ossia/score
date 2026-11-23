#pragma once
#include <Threedim/PatternCapture.hpp>
#include <Threedim/StructuredLightCore.hpp>

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{
//! One Structured Light Calibration run, the library of the Structured Light
//! Calibration process and of the calibration tool. It shows the patterns one
//! projector after the other (image(), one image per projector), captures the
//! camera frames the caller passes to tick() once each pattern has settled,
//! and computes each projector on the executor as soon as its capture ends.
//! It owns no port and no window, and all its methods are called from one
//! thread.
class StructuredLightSession : public StructuredLightCore
{
public:
  //! Read on every tick; holds no string, so building it costs nothing.
  struct Settings
  {
    int projectorWidth{1920}, projectorHeight{1080};
    int projectors{1};
    int mode{DepthCamera};
    float fov{60.f};
    CameraLens lens;
    std::array<float, 4> cameraIntrinsics{};
    std::array<float, 16> worldFromCamera{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
    float level{1.f};
    int minStripe{2};
    double settleMs{400.};
    int frames{2};
    //! A pattern whose camera frame does not come, or does not change, within
    //! this fails.
    double timeoutMs{5000.};
    float contrast{0.15f};
    float minSignal{0.05f};
    int minBits{6};
    int holeRadius{24};
    float depthJump{0.05f};
    float ransac{};
    int idle{Black};
    int surface{Planes};
    int maxPlanes{2};
    float planeThreshold{0.02f};
    bool phaseShift{true};
    int phaseSteps{4};
    int phasePeriod{};
    float projectorGamma{2.2f};
    float phaseMinModulation{0.1f};
    int intrinsicsMode{SolveIntrinsics};
    std::array<float, 4> lockedIntrinsics{};
    float throwRatio{};
    std::array<float, 2> lensShift{};
    bool accumulateIntrinsics{};
    float cameraGamma{1.f};
    float captureExposure{};
    double exposureSettleMs{1500.};
    //! Sent on Auto exposure when the run ends: the camera's state before it.
    bool autoExposureAfter{true};
    bool keepPixels{};
  };

  //! What only start() reads.
  struct Run
  {
    //! Where to save the results once the run is done; empty = not saved.
    QString autoSave;
  };

  //! The latest camera frame, and the depth camera's positions when it has them.
  struct Frame
  {
    CameraFrame camera;
    const float* positions{};
    int positionsWidth{}, positionsHeight{};
  };

  //! What the camera should do, and whether a run just completed.
  struct Events
  {
    std::optional<float> exposure;
    std::optional<bool> autoExposure;
    bool done{};
  };

  //! A projector-sized RGBA8 image; version changes with the content.
  struct Display
  {
    int width{}, height{};
    int projector{};
    int projectors{1};
    bool idle{true};
    std::vector<uint8_t> rgba;
    int version{};
  };

  //! Runs a job off the calling thread; the default runs it in place.
  using Executor = std::function<void(std::function<void()>)>;

  StructuredLightSession();
  StructuredLightSession(const StructuredLightSession&) = delete;
  StructuredLightSession& operator=(const StructuredLightSession&) = delete;
  ~StructuredLightSession();

  void setExecutor(Executor e) { m_executor = std::move(e); }

  //! Begins a run; false, with status(), when it cannot run.
  bool start(const Settings& s, const Run& run, double nowMs, Events& ev);
  void abort(const Settings& s, Events& ev, const std::string& status = "Aborted");
  //! One step at wall-clock time nowMs, with the latest camera frame (or none).
  void tick(const Settings& s, double nowMs, const Frame& frame, Events& ev);

  bool running() const noexcept { return m_phase != Phase::Idle; }
  bool capturing() const noexcept { return m_phase == Phase::Capturing; }
  //! Whether the camera is read now: only while a frame is wanted.
  bool wantsCamera() const noexcept;
  bool wantsPositions() const noexcept;
  const std::string& status() const noexcept { return m_status; }
  float progress() const noexcept { return m_progress; }

  //! The image of projector p: its pattern while it is captured, black while
  //! another one is, the idle pattern otherwise.
  const Display& image(int p) const noexcept;
  int images() const noexcept { return int(m_images.size()); }
  //! The image of the projector being captured, or the idle one.
  const Display& display() const noexcept { return image(m_projector); }
  //! The projector being captured.
  int projector() const noexcept { return m_projector; }
  int projectors() const noexcept { return m_projectors; }
  const sl::GraySequence& sequence() const noexcept { return m_seq; }

  //! Results of the last run (or the loaded file), projector 0 first.
  const std::vector<std::shared_ptr<const Result>>& results() const noexcept
  {
    return m_results;
  }
  int resultVersion() const noexcept { return m_resultVersion; }

  //! Reads a calibration on the executor; it replaces the results once read.
  void load(const QString& path);
  //! Writes every result (path, path_p1, ...) on the executor; status() says
  //! how it went once done.
  void save(const QString& path);
  bool busy() const noexcept;

  std::array<float, 4> accumulatedIntrinsics() const noexcept { return m_accumulatedK; }
  //! The camera latency the last run measured, ms; 0 when none.
  double latency() const noexcept { return m_latency; }

  void cancelComputation() noexcept;

  struct Outbox;

private:
  enum class Phase
  {
    Idle,
    Capturing,
    Computing
  };
  void post(std::function<void()> job);
  void collect(const Settings& s, Events& ev);
  void note(const Result& r, bool accumulate);
  void beginProjector(const Settings& s, double now);
  void beginPattern(const Settings& s, double now, Events& ev);
  void endProjector(const Settings& s, double now, Events& ev, std::string failed);
  void submit(Job job);
  void finishRun(const Settings& s, Events& ev);
  void resolveIntrinsics(const Settings& s, Job& job) const;
  int frameCount() const noexcept { return m_seq.count(); }
  void stop(const Settings& s, const std::string& status, Events& ev);
  void showPattern(int k, float level);
  void showIdle(const Settings& s);
  void accumulatePositions(const Settings& s, const Frame& f);

  Executor m_executor;
  std::shared_ptr<Outbox> m_outbox;
  uint64_t m_generation{};

  Phase m_phase{Phase::Idle};
  std::string m_status;
  float m_progress{};

  sl::GraySequence m_seq;
  Job m_job;
  PatternCapture m_capture;
  std::array<std::vector<float>, 3> m_linear;
  int m_k{};
  int m_projector{};
  int m_projectors{1};
  int m_framesBefore{}, m_framesTotal{1};
  double m_settleMs{};
  double m_latency{};
  float m_whiteMean{};
  float m_exposureSent{};
  int m_submitted{}, m_pending{};
  std::shared_ptr<std::atomic_bool> m_cancel;
  std::vector<std::optional<Result>> m_runResults;
  QString m_autoSave;

  std::vector<std::shared_ptr<const Result>> m_results;
  int m_resultVersion{};
  std::array<float, 4> m_loadedK{};
  std::array<float, 4> m_lastSolvedK{};
  std::vector<std::array<float, 4>> m_solvedKs;
  std::array<float, 4> m_accumulatedK{};

  std::vector<Display> m_images;
  int m_shown{-2};
  int m_shownIdle{-1};
  float m_shownLevel{-1.f};
};
}
