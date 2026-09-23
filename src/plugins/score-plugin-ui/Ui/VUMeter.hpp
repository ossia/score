#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Telemetry.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/widgets/LevelMeter.hpp>

#include <ossia/dataflow/telemetry.hpp>

#include <QPainter>
#include <QPointer>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Ui::VUMeter
{
struct Node
{
  halp_meta(name, "VU Meter")
  halp_meta(c_name, "VUMeter")
  halp_meta(category, "Monitoring")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Multi-channel audio level meter with peak and RMS display")
  halp_meta(uuid, "0d0a3152-8ee9-4472-8a97-457b8bd6e56a")
  halp_flag(fully_custom_item);

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
  } inputs;

  struct
  {
    // Levels output: interleaved [peak0, rms0, peak1, rms1, ...]
    struct : halp::val_port<"Levels", std::vector<float>>
    {
      enum widget
      {
        control
      };
    } levels;
  } outputs;

  halp::setup setup_info{};

  // Per-channel envelope state
  struct ChannelState
  {
    double peak_env = 0.0;
    double rms_env = 0.0;
    double peak_hold = 0.0;
    int peak_hold_counter = 0;
  };
  std::vector<ChannelState> channel_states;

  // Release time in seconds — controls how fast the bars fall.
  // Decrease for snappier response, increase for smoother visuals.
  static constexpr double release_time = 0.150;  // 150ms release
  static constexpr int peak_hold_buffers = 20;    // ~20 buffers hold time
  static constexpr double peak_hold_decay = 0.90; // decay factor after hold expires

  void prepare(halp::setup s) noexcept
  {
    setup_info = s;
    channel_states.clear();
  }

  void operator()(int frames)
  {
    const int channels = inputs.audio.channels;
    if(channels == 0)
      return;

    // Resize state if channel count changed
    if(std::ssize(channel_states) != channels)
      channel_states.resize(channels);

    // Per-buffer release coefficient: accounts for buffer size so decay
    // speed is independent of buffer size and sample rate.
    const double rate = setup_info.rate > 0 ? setup_info.rate : 48000.0;
    const double release = std::exp(-frames / (release_time * rate));

    // Written in place: the vector keeps its capacity from one tick to the next.
    auto& level_data = outputs.levels.value;
    level_data.clear();

    for(int c = 0; c < channels; c++)
    {
      auto in = inputs.audio.channel(c, frames);
      auto& state = channel_states[c];

      // Compute peak and RMS for this buffer
      double buf_peak = 0.0;
      double buf_rms_sum = 0.0;
      for(int i = 0; i < frames; i++)
      {
        const double s = std::abs(in[i]);
        buf_peak = std::max(buf_peak, s);
        buf_rms_sum += in[i] * in[i];
      }
      const double buf_rms = std::sqrt(buf_rms_sum / frames);

      // Envelope following: instant attack, exponential release
      if(buf_peak >= state.peak_env)
        state.peak_env = buf_peak;
      else
        state.peak_env *= release;

      if(buf_rms >= state.rms_env)
        state.rms_env = buf_rms;
      else
        state.rms_env *= release;

      // Peak hold with timed decay
      if(buf_peak >= state.peak_hold)
      {
        state.peak_hold = buf_peak;
        state.peak_hold_counter = peak_hold_buffers;
      }
      else if(state.peak_hold_counter > 0)
      {
        state.peak_hold_counter--;
      }
      else
      {
        state.peak_hold *= peak_hold_decay;
      }

      level_data.push_back(static_cast<float>(state.peak_env));
      level_data.push_back(static_cast<float>(state.rms_env));
      level_data.push_back(static_cast<float>(state.peak_hold));
    }
  }

  //! Shows what the audio inlet receives, read from the execution's
  //! telemetry, at the rate and under the setting every feedback follows.
  struct Layer : public Process::EffectLayerView
  {
  public:
    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_telemetry{&doc.plugin<Execution::DocumentPlugin>().telemetry()}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto* audio_inlet = process.inlets().front();
      auto fact = portFactory.get(audio_inlet->concreteKey());
      auto port = fact->makePortItem(*audio_inlet, doc, this, this);
      port->setPos(0, 5);

      m_handle = m_telemetry->meterInlet(*safe_cast<Process::AudioInlet*>(audio_inlet));
      connect(m_telemetry, &Execution::Telemetry::updated, this, [this] {
        const auto* levels = m_telemetry->levels(m_handle);
        if(!levels)
        {
          m_meter.setInactive();
        }
        else
        {
          m_levels.resize(levels->channels);
          for(std::size_t c = 0; c < levels->channels; c++)
            m_levels[c]
                = {levels->peak[c], levels->rms(c), levels->is_clipped(c)};
          m_meter.setLevels(m_levels);
        }
        update();
      });
    }

    ~Layer() override
    {
      if(m_telemetry)
        m_telemetry->release(m_handle);
    }

    void paint_impl(QPainter* p) const override
    {
      // Room on the left for the inlet.
      m_meter.paint(*p, boundingRect().adjusted(14., 4., -2., -4.), true);
    }

  private:
    QPointer<Execution::Telemetry> m_telemetry;
    Execution::Telemetry::Meter m_handle;
    score::LevelMeterState m_meter;
    std::vector<score::LevelMeterState::Channel> m_levels;
  };
};
}
