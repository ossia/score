#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Monitoring/MonitorLink.hpp>
#include <Process/Monitoring/SampleRing.hpp>

#include <Effect/EffectLayer.hpp>

#include <Ui/Monitor/SpectrumAnalysis.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <memory>
#include <vector>

/**
 * A tuner: the pitch of the signal, by YIN, as a note, its frequency and how
 * many cents it is off, against a reference for A4.
 */
namespace Ui::Tuner
{
struct TunerMonitor final : Process::Monitor
{
  Process::SampleRing ring{1 << 14};
};

struct Layer;

struct Node
{
  halp_meta(name, "Tuner")
  halp_meta(c_name, "Tuner")
  halp_meta(category, "Monitoring/Audio")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Shows the note the signal plays, and how many cents it is off")
  halp_meta(uuid, "bc22f3ed-7ee8-4de4-80c4-cae2f372b2d7")
  halp_flag(fully_custom_item);
  using Layer = Ui::Tuner::Layer;

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::spinbox_f32<"A4", halp::range{400., 480., 440.}>
    {
      halp_meta(description, "Frequency of the A above middle C, in Hz")
    } reference;
  } inputs;

  struct
  {
  } outputs;

  std::shared_ptr<TunerMonitor> monitor = std::make_shared<TunerMonitor>();

  void prepare(halp::setup s) { monitor->ring.set_rate(s.rate > 0 ? s.rate : 48000.); }

  void operator()(int frames)
  {
    const int nch = inputs.audio.channels;
    if(nch > 0 && frames > 0 && monitor->watched())
      monitor->ring.write(nch, frames, [&](int c) { return inputs.audio[c]; });
  }
};

struct Layer : public Process::EffectLayerView
{
  //! YIN costs the square of the frame: 2048 samples reach 23 Hz at 48 kHz
  static constexpr std::size_t frame = 2048;

  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void paint_impl(QPainter* p) const override;

private:
  void refresh(TunerMonitor& mon);
  double reference() const;

  const Process::ControlInlet* m_reference{};
  Process::MonitorLink* m_link{};
  Ui::Monitor::PitchTracker m_pitch;
  std::vector<std::vector<float>> m_in;
  std::vector<float> m_mix;
  double m_freq{};
  bool m_signal{};
};
}
