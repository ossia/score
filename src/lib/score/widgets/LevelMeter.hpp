#pragma once
#include <QElapsedTimer>
#include <QWidget>

#include <score_lib_base_export.h>

#include <span>
#include <vector>

class QPainter;
namespace score
{
/**
 * A vertical multichannel level meter, in dBFS, independent of what shows it.
 *
 * Each call to setLevels() brings what happened since the previous one: the
 * meter keeps a falling peak, a peak hold and a clip mark per channel, timed
 * on the wall clock so that it behaves the same at any update rate.
 *
 * It lays out the widest channel count seen over the last seconds, so that a
 * signal whose width changes does not make it jump. Channels too narrow for a
 * bar are drawn as a heat strip, one column per channel, next to a bar of
 * their loudest.
 */
class SCORE_LIB_BASE_EXPORT LevelMeterState
{
public:
  struct Channel
  {
    float peak{}; //!< linear, since the previous update
    float rms{};  //!< linear, over the same span
    bool clipped{};
  };

  LevelMeterState();

  void setLevels(std::span<const Channel> channels);
  //! Nothing is running: the meter keeps its layout and dims.
  void setInactive();

  //! Channels currently laid out.
  int channelCount() const noexcept { return int(m_state.size()); }
  bool active() const noexcept { return m_active; }
  bool anyClipped() const noexcept;
  void resetClips();

  //! Draws in `r`, with a dB scale on the left when asked and when it fits.
  void paint(QPainter& p, const QRectF& r, bool scale) const;

private:
  struct State
  {
    float peak_db{-200.f};
    float rms_db{-200.f};
    float hold_db{-200.f};
    double hold_since{};
    bool clipped{};
  };

  std::vector<State> m_state;
  QElapsedTimer m_clock;
  double m_last{};
  double m_widestSince{};
  bool m_active{};
};

//! A LevelMeterState in a widget. A click clears the clip marks.
class SCORE_LIB_BASE_EXPORT LevelMeter : public QWidget
{
public:
  using Channel = LevelMeterState::Channel;

  explicit LevelMeter(QWidget* parent = nullptr);
  ~LevelMeter() override;

  void setLevels(std::span<const Channel> channels);
  void setInactive();

  int channelCount() const noexcept { return m_meter.channelCount(); }
  bool anyClipped() const noexcept { return m_meter.anyClipped(); }
  void resetClips();

  void setScaleVisible(bool b);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;

private:
  LevelMeterState m_meter;
  bool m_scale{true};
};
}
