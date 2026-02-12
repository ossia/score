#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QPainter>

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
    halp::dynamic_audio_bus<"Audio", double> audio;

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

    std::vector<float> level_data;
    level_data.reserve(channels * 3);

    for(int c = 0; c < channels; c++)
    {
      auto in = inputs.audio.channel(c, frames);
      auto& state = channel_states[c];

      // Pass audio through
      if(c < outputs.audio.channels)
      {
        auto out = outputs.audio.channel(c, frames);
        for(int i = 0; i < frames; i++)
          out[i] = in[i];
      }

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

    outputs.levels.value = std::move(level_data);
  }

  // dB conversion utilities used by the Layer
  static double to_dB(double linear) noexcept
  {
    if(linear <= 1e-10)
      return -100.0;
    return 20.0 * std::log10(linear);
  }

  static double dB_to_y(double dB, double height, double min_dB = -60.0) noexcept
  {
    // Map dB range [min_dB, 0] to y range [height, 0]
    const double clamped = std::clamp(dB, min_dB, 0.0);
    return height * (1.0 - (clamped - min_dB) / (0.0 - min_dB));
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    static constexpr double min_dB = -60.0;
    static constexpr double preferred_strip_width = 16.0;
    static constexpr double min_strip_spacing = 1.0;
    static constexpr double preferred_strip_spacing = 3.0;
    static constexpr double label_width = 30.0;
    static constexpr double top_margin = 4.0;
    static constexpr double bottom_margin = 12.0;
    // Hide channel labels when strips are narrower than this
    static constexpr double min_width_for_labels = 8.0;

    // Per-channel display data: peak, rms, peak_hold (linear)
    struct ChannelDisplay
    {
      float peak = 0.f;
      float rms = 0.f;
      float peak_hold = 0.f;
    };
    std::vector<ChannelDisplay> m_channels;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto* audio_inlet = process.inlets().front();
      auto fact = portFactory.get(audio_inlet->concreteKey());
      auto port = fact->makePortItem(*audio_inlet, doc, this, this);
      port->setPos(0, 5);

      // Find the levels outlet (last outlet)
      auto* levels_outlet
          = static_cast<Process::ControlOutlet*>(process.outlets().back());
      connect(
          levels_outlet, &Process::ControlOutlet::valueChanged, this,
          [this](const ossia::value& v) {
        if(auto* list = v.target<std::vector<ossia::value>>())
        {
          const int entries = list->size();
          // Data is interleaved: [peak0, rms0, hold0, peak1, rms1, hold1, ...]
          const int num_channels = entries / 3;
          m_channels.resize(num_channels);
          for(int c = 0; c < num_channels; c++)
          {
            m_channels[c].peak = ossia::convert<float>((*list)[c * 3 + 0]);
            m_channels[c].rms = ossia::convert<float>((*list)[c * 3 + 1]);
            m_channels[c].peak_hold = ossia::convert<float>((*list)[c * 3 + 2]);
          }
          update();
        }
      });
    }

    void reset()
    {
      m_channels.clear();
      update();
    }

    static QColor level_color(double dB) noexcept
    {
      // Green below -12dB, yellow from -12 to -3dB, red above -3dB
      if(dB < -12.0)
        return QColor(76, 175, 80);   // green
      else if(dB < -3.0)
      {
        // Interpolate green -> yellow
        const double t = (dB + 12.0) / 9.0;
        return QColor(
            76 + static_cast<int>(179 * t),
            175 + static_cast<int>(80 * t - 80 * t * t * 0.3),
            80 - static_cast<int>(60 * t));
      }
      else
      {
        // Interpolate yellow -> red
        const double t = std::min(1.0, (dB + 3.0) / 3.0);
        return QColor(
            255,
            static_cast<int>(235 * (1.0 - t)),
            static_cast<int>(20 * (1.0 - t)));
      }
    }

    void paint_impl(QPainter* p) const override
    {
      const int num_ch = m_channels.size();
      if(num_ch == 0)
        return;

      const auto bounds = boundingRect();
      const double total_h = bounds.height();
      const double meter_h = total_h - top_margin - bottom_margin;
      if(meter_h <= 0)
        return;

      p->save();
      p->setRenderHint(QPainter::Antialiasing, false);

      const double start_x = label_width + 4.0;
      const double avail_w = bounds.width() - start_x;

      // Compute adaptive strip width and spacing
      double sw = preferred_strip_width;
      double sp = preferred_strip_spacing;
      const double ideal_w = num_ch * sw + (num_ch - 1) * sp;
      if(ideal_w > avail_w && num_ch > 0)
      {
        // Shrink spacing first, then strip width
        sp = min_strip_spacing;
        sw = (avail_w - (num_ch - 1) * sp) / num_ch;
        if(sw < 1.0)
        {
          sp = 0.0;
          sw = avail_w / num_ch;
        }
      }

      if(sw <= 0)
      {
        p->restore();
        return;
      }

      // Draw dB scale on left
      draw_scale(p, start_x - 2.0, meter_h);

      const bool show_labels = sw >= min_width_for_labels;

      // Draw each channel strip
      for(int c = 0; c < num_ch; c++)
      {
        const double x = start_x + c * (sw + sp);
        draw_channel_strip(p, x, sw, meter_h, m_channels[c]);

        if(show_labels)
        {
          p->setPen(QColor(180, 180, 180));
          QFont f = p->font();
          f.setPixelSize(9);
          p->setFont(f);
          const auto label = QString::number(c);
          p->drawText(
              QRectF(x, top_margin + meter_h + 1, sw, bottom_margin - 1),
              Qt::AlignHCenter | Qt::AlignTop, label);
        }
      }

      p->restore();
    }

  private:
    void draw_scale(QPainter* p, double right_x, double meter_h) const
    {
      p->setPen(QColor(120, 120, 120));
      QFont f = p->font();
      f.setPixelSize(8);
      p->setFont(f);

      static constexpr double dB_marks[]
          = {0, -3, -6, -12, -18, -24, -36, -48, -60};
      for(double dB : dB_marks)
      {
        const double y = top_margin + dB_to_y(dB, meter_h, min_dB);
        const auto label = (dB == 0) ? QStringLiteral(" 0")
                                     : QString::number(static_cast<int>(dB));

        p->drawText(
            QRectF(0, y - 5, right_x - 2, 10), Qt::AlignRight | Qt::AlignVCenter,
            label);

        // Tick mark
        p->drawLine(QPointF(right_x - 1, y), QPointF(right_x + 1, y));
      }
    }

    void draw_channel_strip(
        QPainter* p, double x, double sw, double meter_h,
        const ChannelDisplay& ch) const
    {
      // Background
      p->fillRect(
          QRectF(x, top_margin, sw, meter_h), QColor(20, 20, 20));

      const double peak_dB = Node::to_dB(ch.peak);
      const double rms_dB = Node::to_dB(ch.rms);
      const double hold_dB = Node::to_dB(ch.peak_hold);

      // Draw RMS bar (full strip width minus 2px margin)
      const double rms_w = std::max(1.0, sw - 2);
      draw_level_bar(p, x + 1, meter_h, rms_dB, 0.6, rms_w);

      // Draw peak bar (narrower, overlaid) — only if strip is wide enough
      if(sw >= 6.0)
      {
        const double peak_bar_w = std::max(1.0, sw - 6);
        const double peak_x = x + 3;
        draw_level_bar(p, peak_x, meter_h, peak_dB, 1.0, peak_bar_w);
      }
      else
      {
        // Strip too narrow for two layers, just draw peak
        draw_level_bar(p, x, meter_h, peak_dB, 1.0, sw);
      }

      // Draw peak hold indicator
      if(hold_dB > min_dB)
      {
        const double hold_y = top_margin + dB_to_y(hold_dB, meter_h, min_dB);
        p->setPen(Qt::NoPen);
        const double hold_margin = std::min(1.0, sw * 0.1);
        p->fillRect(
            QRectF(x + hold_margin, hold_y - 1, sw - 2 * hold_margin, 2),
            level_color(hold_dB));
      }
    }

    void draw_level_bar(
        QPainter* p, double x, double meter_h, double level_dB, double alpha,
        double bar_w) const
    {
      if(level_dB <= min_dB)
        return;

      // Draw in segments for color gradient effect
      static constexpr double segment_dB_step = 1.5;
      double current_dB = min_dB;

      p->setPen(Qt::NoPen);
      while(current_dB < level_dB)
      {
        const double seg_top_dB = std::min(current_dB + segment_dB_step, level_dB);
        const double y_bottom
            = top_margin + dB_to_y(current_dB, meter_h, min_dB);
        const double y_top
            = top_margin + dB_to_y(seg_top_dB, meter_h, min_dB);

        QColor col = level_color(seg_top_dB);
        col.setAlphaF(alpha);
        p->fillRect(QRectF(x, y_top, bar_w, y_bottom - y_top), col);

        current_dB = seg_top_dB;
      }
    }

    static double dB_to_y(double dB, double height, double min_dB) noexcept
    {
      const double clamped = std::clamp(dB, min_dB, 0.0);
      return height * (1.0 - (clamped - min_dB) / (0.0 - min_dB));
    }
  };
};
}
