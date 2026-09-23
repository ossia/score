#pragma once
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/LevelMeter.hpp>

#include <Execution/Telemetry.hpp>

#include <QPointer>
#include <QWidget>

#include <nano_signal_slot.hpp>

#include <utility>
#include <vector>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
namespace ossia
{
class audio_parameter;
namespace net
{
class node_base;
}
namespace telemetry
{
struct meter_levels;
}
}
namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class IntervalModel;
}

namespace Dataflow
{
class AudioDevice;
}

namespace Mixer
{
//! Asks for a new mapped or virtual port of the audio device and adds it,
//! showing the device in the device explorer first if it is not there yet.
void addAudioPort(
    const score::DocumentContext& ctx, Dataflow::AudioDevice& dev, QWidget* parent);

enum class StripWidth
{
  Narrow,
  Normal,
  Wide
};

//! A vertical fader in dB: position p gives a gain of p³, 0 dB at the top.
class GainFader final : public score::DoubleSlider
{
public:
  explicit GainFader(QWidget* parent);

  static double positionToGain(double p) noexcept;
  static double gainToPosition(double g) noexcept;

  double map(double position) const override;
  double unmap(double db) const override;

protected:
  void paintEvent(QPaintEvent*) override;
};

//! Balance between the first two channels: centre is unity on both.
class PanSlider final : public score::DoubleSlider
{
public:
  explicit PanSlider(QWidget* parent);

  static std::pair<double, double> weights(double position) noexcept;
  static double position(double left, double right) noexcept;

protected:
  void paintEvent(QPaintEvent*) override;
};

//! What every strip has: a name, a meter next to a fader, and a value readout.
class Strip : public QWidget
{
public:
  Strip(const score::DocumentContext& ctx, QWidget* parent);
  ~Strip() override;

  void setStripWidth(StripWidth w);
  StripWidth stripWidth() const noexcept { return m_width; }

  //! Reads the strip's meter from the latest telemetry update.
  virtual void updateMeter(const Execution::Telemetry& t);
  //! Picks up values that do not notify, e.g. a device gain set by automation.
  virtual void poll() { }

  void setHighlighted(bool b);

protected:
  //! Shows the meter's channels, or only the listed ones.
  void setMeter(Execution::Telemetry::Meter m, std::vector<int> channels = {});
  void setGainReadout(double gain);
  void contextMenuEvent(QContextMenuEvent*) override;
  void paintEvent(QPaintEvent*) override;
  virtual void fillContextMenu(class QMenu&) { }

  const score::DocumentContext& m_context;
  QVBoxLayout* m_layout{};
  QPushButton* m_title{};
  QLabel* m_badge{};
  score::LevelMeter* m_meter{};
  GainFader* m_fader{};
  QLabel* m_readout{};
  QWidget* m_buttons{};
  QPointer<Execution::Telemetry> m_telemetry;

private:
  Execution::Telemetry::Meter m_meterHandle;
  std::vector<int> m_channels;
  std::vector<score::LevelMeter::Channel> m_levels;
  StripWidth m_width{StripWidth::Normal};
  bool m_highlighted{};
};

//! An interval marked as a bus.
class BusStrip final : public Strip
{
public:
  BusStrip(
      const Scenario::IntervalModel& itv, const score::DocumentContext& ctx,
      QWidget* parent);
  ~BusStrip() override;

  const Scenario::IntervalModel& interval() const noexcept { return m_model; }

  //! Also shows the CPU share of every process in the bus, when measured.
  void updateMeter(const Execution::Telemetry& t) override;

private:
  void fillContextMenu(QMenu&) override;
  void syncFromModel();

  const Scenario::IntervalModel& m_model;
  QToolButton* m_mute{};
  QToolButton* m_solo{};
  QToolButton* m_propagate{};
  PanSlider* m_pan{};
};

//! A parameter of the audio device: a hardware channel, the main input or
//! output, a mapped or a virtual port.
class PortStrip final
    : public Strip
    , public Nano::Observer
{
public:
  enum class Meter
  {
    None,
    HardwareInputs,
    HardwareOutputs,
  };
  //! `channels`: which channels of the hardware meter are this port's, all
  //! of them when empty.
  PortStrip(
      ossia::audio_parameter& param, Meter meter, std::vector<int> channels,
      const score::DocumentContext& ctx, QWidget* parent);
  ~PortStrip() override;

  void poll() override;

private:
  void fillContextMenu(QMenu&) override;
  void onNodeRemoved(const ossia::net::node_base&);

  ossia::audio_parameter* m_param{};
  bool m_dragging{};
};
}
