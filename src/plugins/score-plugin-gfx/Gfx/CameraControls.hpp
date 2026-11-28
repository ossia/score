#pragma once
#include <score_plugin_gfx_export.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ossia
{
class value;
namespace net
{
class node_base;
class parameter_base;
}
}

namespace Gfx
{
struct CameraSettings;

//! The names of a camera's controls under <device>:/controls/color, shared
//! with the depth camera add-on. Exposure is in microseconds.
namespace CameraControlNames
{
inline constexpr const char* root = "/controls/color/";
inline constexpr const char* exposure = "exposure";
inline constexpr const char* auto_exposure = "auto_exposure";
inline constexpr const char* focus = "focus";
inline constexpr const char* auto_focus = "auto_focus";
inline constexpr const char* gain = "gain";
inline constexpr const char* white_balance = "white_balance";
inline constexpr const char* auto_white_balance = "auto_white_balance";
inline constexpr const char* backlight_compensation = "backlight_compensation";
}

//! A camera's hardware controls, as one platform API reaches them. Every call
//! is made on the CameraControls thread, open() first and close() last.
class SCORE_PLUGIN_GFX_EXPORT CameraControlBackend
{
public:
  struct Control
  {
    std::string name;
    std::string auto_name;
    double min{}, max{};
    bool is_float{};
  };
  struct State
  {
    double value{};
    bool automatic{};
  };

  virtual ~CameraControlBackend();
  //! The controls the camera has; empty when it cannot be reached.
  virtual std::vector<Control> open() = 0;
  virtual std::optional<State> get(std::size_t control) = 0;
  //! In the units of Control: the backend rounds and clamps to the device.
  virtual bool set(std::size_t control, double value, bool automatic) = 0;
  virtual void close() = 0;
};

//! Publishes a backend's controls as parameters and drives the camera from
//! them on a thread of its own: writes from any thread are merged per
//! control, so the camera sees the latest value only; controls on auto are
//! polled; and for a few seconds after the capture starts, the controls that
//! were written are compared to the device and set again, since a camera may
//! go back to values of its own when its stream opens.
class SCORE_PLUGIN_GFX_EXPORT CameraControls
{
public:
  using clock = std::chrono::steady_clock;
  static constexpr auto poll_interval = std::chrono::milliseconds(500);
  static constexpr auto enforce_interval = std::chrono::milliseconds(100);
  static constexpr auto enforce_duration = std::chrono::seconds(5);

  //! With `threaded` false, nothing runs until step() is called: for tests.
  explicit CameraControls(
      std::unique_ptr<CameraControlBackend> backend, bool threaded = true);
  ~CameraControls();
  CameraControls(const CameraControls&) = delete;
  CameraControls& operator=(const CameraControls&) = delete;

  bool empty() const noexcept { return m_controls.empty(); }

  void setup(ossia::net::node_base& root);
  bool push(const ossia::net::parameter_base&, const ossia::value&);

  //! The capture started: a camera may reset itself when its stream opens.
  void streamStarted();

  //! Must be called before the nodes given to setup() are destroyed.
  void stop();

  //! One turn of the thread: the pending writes, then a poll when due.
  void step(clock::time_point now);

  struct Wanted
  {
    bool written{};
    double value{};
    bool automatic{};
    double applied{};
  };
  //! Whether a control the device reports in `read` must be set again.
  static bool needsReapply(
      const Wanted& w, CameraControlBackend::State read, bool hasAuto) noexcept;

private:
  struct Pending
  {
    bool value{}, mode{};
    double v{};
    bool automatic{};
  };
  struct Control
  {
    std::size_t index{};
    CameraControlBackend::Control info;
    CameraControlBackend::State state;
    Wanted wanted;
    Pending pending;
    ossia::net::parameter_base* param{};
    ossia::net::parameter_base* auto_param{};
  };

  void load();
  void run();
  void write(Control& c, const Pending& p);
  void refresh(bool enforce);
  void publish(Control& c);

  std::unique_ptr<CameraControlBackend> m_backend;
  std::vector<Control> m_controls;

  std::mutex m_mutex;
  std::condition_variable m_cv;
  bool m_running{true};
  bool m_dirty{};
  std::atomic_bool m_published{};
  bool m_closed{};
  bool m_lost{};
  int m_failedPolls{};
  clock::time_point m_enforce_until{};
  clock::time_point m_next_poll{};
  std::thread m_thread;
};

//! Null when the camera cannot be found or has no control.
SCORE_PLUGIN_GFX_EXPORT
std::shared_ptr<CameraControls> make_camera_controls(const CameraSettings&);

namespace dshow
{
//! DirectShow counts exposure in whole log2 seconds.
inline std::optional<long> exposureStop(double microseconds, long min, long max) noexcept
{
  if(!(microseconds > 0.) || !std::isfinite(microseconds))
    return std::nullopt;
  const double stop = std::round(std::log2(microseconds * 1e-6));
  return long(std::clamp(stop, double(min), double(max)));
}

inline double exposureMicroseconds(long stop) noexcept
{
  return std::exp2(double(stop)) * 1e6;
}
}
}
