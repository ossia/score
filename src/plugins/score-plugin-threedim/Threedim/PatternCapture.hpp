#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace Threedim
{
//! An RGBA float camera frame. `serial` counts the frames the camera
//! delivered when the caller knows it; 0 = unknown, then any frame at least
//! Options::intervalMs after the previous one counts as new.
struct CameraFrame
{
  const float* rgba{};
  int width{}, height{};
  uint64_t serial{};
};

//! The capture of one shown pattern: waits for it to settle, then averages
//! the frames that follow, and gives up when none comes. Structured light
//! and LED discovery capture their patterns through it.
class PatternCapture
{
public:
  struct Options
  {
    double settleMs{400.};
    int frames{1};
    //! Without an accepted frame for this long, the capture fails.
    double timeoutMs{5000.};
    double intervalMs{30.};
    //! The first frame must differ from the last one of the previous pattern.
    bool requireChange{};
    //! The first frame's mean luminance must be under this; < 0: off.
    float darkerThan{-1.f};
    //! Per channel, raw camera value -> linear, sampled at i / (size - 1).
    const std::array<std::vector<float>, 3>* linearise{};
    //! Keep the average raw RGB too.
    bool rgb{};
    //! Raw values at or above are clipped.
    float clipLevel{2.f};
  };
  enum class State
  {
    Settling,
    Capturing,
    Done,
    //! No frame came.
    NoFrame,
    //! Frames came, none changed as the pattern did.
    Unchanged,
    SizeChanged
  };

  //! A new run: the next frames may have any size, and nothing to change from.
  void reset() noexcept;
  //! The pattern was shown at `now`.
  void begin(double now, const Options& o);
  //! One tick, with the latest frame (rgba null when none).
  State feed(double now, const CameraFrame& f);

  State state() const noexcept { return m_state; }
  bool failed() const noexcept { return m_state >= State::NoFrame; }
  bool wantsFrame() const noexcept { return m_state == State::Capturing; }
  //! Whether the last feed took its frame.
  bool accepted() const noexcept { return m_accepted; }
  int width() const noexcept { return m_w; }
  int height() const noexcept { return m_h; }

  //! When Done: mean linear luminance, raw RGB (Options::rgb), clipped pixels.
  std::vector<float>& luminance() noexcept { return m_lum; }
  std::vector<float>& rgb() noexcept { return m_rgb; }
  std::vector<uint8_t>& clipped() noexcept { return m_clipped; }
  //! Mean of luminance().
  float mean() const noexcept { return m_mean; }
  //! From the pattern shown to its first frame taken.
  double latency() const noexcept { return m_latency; }

  //! Raw -> linear table of a gamma.
  static std::array<std::vector<float>, 3> gammaTable(float gamma, int size = 4096);

private:
  float linear(int c, float v) const noexcept;
  uint64_t signature(const CameraFrame& f) const noexcept;
  float sampledMean(const CameraFrame& f) const noexcept;

  Options m_opt;
  State m_state{State::Done};
  double m_shownAt{}, m_waitFrom{}, m_lastAt{-1e300};
  uint64_t m_lastSerial{};
  int m_w{}, m_h{};
  int m_taken{};
  bool m_accepted{};
  bool m_sawFrame{};
  bool m_hasPrevious{};
  uint64_t m_previous{}, m_last{};
  std::vector<float> m_lum, m_rgb;
  std::vector<uint8_t> m_clipped;
  float m_mean{};
  double m_latency{};
};
}
