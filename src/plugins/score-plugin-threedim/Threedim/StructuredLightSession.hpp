#pragma once
#include <Threedim/StructuredLightCore.hpp>

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{
//! One Structured Light Calibration run: shows the patterns one projector after
//! the other, captures the camera once each has settled, then decodes on a
//! worker thread. It owns no port and no window: the caller shows display() on
//! the projectors and passes the latest camera frame to tick().
class StructuredLightSession : public StructuredLightCore
{
public:
  struct Settings
  {
    int projectorWidth{1920}, projectorHeight{1080};
    int projectors{1};
    int mode{DepthCamera};
    float fov{60.f};
    std::array<float, 4> cameraIntrinsics{};
    std::array<float, 16> worldFromCamera{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
    float level{1.f};
    int minStripe{2};
    double settleMs{400.};
    int frames{2};
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
    int responseLevels{};
    bool compensation{};
    //! Read by start() only.
    std::vector<float> exposures;
    double exposureSettleMs{1500.};
    float captureExposure{};
    std::string crfFile;
    float compGamut{0.85f};
    float compStrength{1.f};
    float compGradient{};
    bool compBlack{};
    int mixLevels{5};
    std::string savePath;
    bool autoSave{};
  };

  //! RGBA float camera frame, and the depth camera's positions when it has them.
  struct Frame
  {
    const float* rgba{};
    int width{}, height{};
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

  //! The image of the projector the run is on (or of every projector, idle),
  //! projector-sized RGBA8; version changes with the content.
  struct Display
  {
    int width{}, height{};
    int projector{};
    int projectors{1};
    bool idle{true};
    std::vector<uint8_t> rgba;
    int version{};
  };

  StructuredLightSession() = default;
  StructuredLightSession(const StructuredLightSession&) = delete;
  StructuredLightSession& operator=(const StructuredLightSession&) = delete;
  ~StructuredLightSession();

  //! Begins a run; false, with status(), when the settings cannot run.
  bool start(const Settings& s, double nowMs, Events& ev);
  void abort(const Settings& s, Events& ev, const std::string& status = "Aborted");
  //! One step at wall-clock time nowMs, with the latest camera frame (or none).
  void tick(const Settings& s, double nowMs, const Frame& frame, Events& ev);

  bool running() const noexcept { return m_phase != Phase::Idle; }
  bool capturing() const noexcept
  {
    return m_phase == Phase::Settling || m_phase == Phase::Capturing;
  }
  bool wantsCamera() const noexcept { return m_wantCamera; }
  bool wantsPositions() const noexcept { return m_wantPositions; }
  const std::string& status() const noexcept { return m_status; }
  void setStatus(std::string s) { m_status = std::move(s); }
  float progress() const noexcept { return m_progress; }

  const Display& display() const noexcept { return m_display; }
  void showPattern(
      const sl::GraySequence& seq, int projector, int projectors, int k, float level,
      const std::vector<std::array<float, 3>>& extraColors);
  void showIdle(int w, int h, int idle, float level, int projectors);
  //! Index of the frame shown, -1 when idle.
  int shown() const noexcept { return m_shown; }
  const sl::GraySequence& sequence() const noexcept { return m_seq; }
  const std::vector<std::array<float, 3>>& extraColors() const noexcept
  {
    return m_extraColors;
  }
  float shownLevel() const noexcept { return m_shownLevel; }
  int projector() const noexcept { return m_projector; }
  int projectors() const noexcept { return m_projectors; }

  //! Every projector's display in its canvas rect ("x y w h;..." normalised).
  void renderCanvas(int W, int H, const std::string& layout, int count, uint8_t* dst) const;

  //! Results of the last run (or the loaded file), projector 0 first.
  const std::vector<std::shared_ptr<const Result>>& results() const noexcept
  {
    return m_results;
  }
  int resultVersion() const noexcept { return m_resultVersion; }
  //! The results another session computed, shown by this one.
  void mirrorResults(std::vector<std::shared_ptr<const Result>> r)
  {
    m_results = std::move(r);
    m_resultVersion++;
  }
  //! A result from a file replaces the run's.
  void adopt(std::shared_ptr<Result> r, bool accumulate);
  //! Writes every result next to `path` (path, path_p1, ...); returns the status.
  std::string saveAll(const std::string& path) const;

  std::array<float, 3> measuredGamma() const noexcept { return m_measuredGamma; }
  std::array<float, 4> accumulatedIntrinsics() const noexcept { return m_accumulatedK; }

  void cancelComputation() noexcept;

private:
  enum class Phase
  {
    Idle,
    Settling,
    Capturing,
    Computing
  };
  void note(const Result& r, bool accumulate);
  void finishProjector(const Settings& s, double now, Events& ev);
  bool captureFrame(const Settings& s, const Frame& f);
  void resolveIntrinsics(const Settings& s, Job& job) const;
  int frameCount() const noexcept { return m_seq.count() + int(m_extras.size()); }
  void stop(const Settings& s, const std::string& status, Events& ev);

  Phase m_phase{Phase::Idle};
  std::string m_status;
  float m_progress{};
  bool m_wantCamera{}, m_wantPositions{};

  sl::GraySequence m_seq;
  Job m_job;
  int m_k{};
  double m_shownAt{};
  double m_lastFrameAt{-1e9};
  int m_framesTaken{};
  std::vector<float> m_frameSum;
  std::vector<float> m_rgbSum;
  int m_camW{}, m_camH{};
  int m_projector{};
  int m_projectors{1};
  int m_framesBefore{}, m_framesTotal{1};
  double m_settleMs{};
  float m_exposureSent{};
  std::vector<Extra> m_extras;
  std::vector<std::array<float, 3>> m_extraColors;
  std::vector<Job> m_pendingJobs;
  std::future<std::vector<Result>> m_future;
  std::shared_ptr<std::atomic_bool> m_cancel;

  std::vector<std::shared_ptr<const Result>> m_results;
  int m_resultVersion{};
  std::shared_ptr<const score::gfx::photometric::CameraResponse> m_loadedCrf;
  std::string m_crfFileLoaded;
  std::array<float, 4> m_loadedK{};
  std::array<float, 4> m_lastSolvedK{};
  std::vector<std::array<float, 4>> m_solvedKs;
  std::array<float, 4> m_accumulatedK{};
  std::array<float, 3> m_measuredGamma{};

  Display m_display;
  int m_shown{-2};
  int m_shownIdle{-1};
  float m_shownLevel{-1.f};
  int m_shownProjector{-1};
  int m_shownProjectors{-1};
};
}
