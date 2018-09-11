#pragma once
#include <Engine/Node/PdNode.hpp>
#include <ossia/detail/math.hpp>
#include <algorithm>
#include <array>
#include <bitset>
#include <map>
#include <random>
#include <tuple>
#include <utility>
namespace Nodes
{

namespace LFO
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "LFO";
    static const constexpr auto objectKey = "LFO";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description = "Low-frequency oscillator";
    static const constexpr auto uuid
        = make_uuid("0697b807-f588-49b5-926c-f97701edd0d8");

    static const constexpr value_out value_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::LFOFreqChooser(),
        Control::FloatSlider{"Amplitude", 0., 1000., 0.},
        Control::FloatSlider{"Fine ampl.", 0., 1., 1.},
        Control::FloatSlider{"Offset", -1000., 1000., 0.},
        Control::FloatSlider{"Fine offset", -1., 1., 0.},
        Control::FloatSlider{"Jitter", 0., 1., 0.},
        Control::FloatSlider{"Phase", -1., 1., 0.},
        Control::Widgets::WaveformChooser());
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
    int64_t phase{};
    std::mt19937 rd;
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void
  run(float freq,
      float ampl,
      float ampl_fine,
      float offset,
      float offset_fine,
      float jitter,
      float phase,
      const std::string& type,
      ossia::value_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& s)
  {
    auto& waveform_map = Control::Widgets::waveformMap();

    if (auto it = waveform_map.find(type); it != waveform_map.end())
    {
      auto ph = s.phase;
      if (jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 5000.)(s.rd) * jitter;
      }

      ampl += ampl_fine;
      offset += offset_fine;

      using namespace Control::Widgets;
      const auto phi = phase + (float(ossia::two_pi) * freq * ph) / st.sampleRate();

      auto add_val = [&] (auto new_val) { out.write_value(ampl * new_val + offset, tk.tick_start()); };
      switch (it->second)
      {
        case Sin:
          add_val(std::sin(phi));
          break;
        case Triangle:
          add_val(std::asin(std::sin(phi)));
          break;
        case Saw:
          add_val(std::atan(std::tan(phi)));
          break;
        case Square:
          add_val((std::sin(phi) > 0.f) ? 1.f : -1.f);
          break;
        case SampleAndHold:
        {
          auto start_phi = phase + (float(ossia::two_pi) * freq * s.phase) / st.sampleRate();
          auto end_phi = phase + (float(ossia::two_pi) * freq * (s.phase + tk.date - tk.prev_date)) / st.sampleRate();
          auto start_s = std::sin(start_phi);
          auto end_s = std::sin(end_phi);
          if((start_s > 0 && end_s <= 0) || (start_s <= 0 && end_s > 0))
          {
            add_val(std::uniform_real_distribution<float>(-1., 1.)(s.rd));
          }
          break;
        }
        case Noise1:
          add_val(std::uniform_real_distribution<float>(-1., 1.)(s.rd));
          break;
        case Noise2:
          add_val(std::normal_distribution<float>(0., 1.)(s.rd));
          break;
        case Noise3:
          add_val(std::cauchy_distribution<float>(0., 1.)(s.rd));
          break;
      }
    }

    s.phase += (tk.date - tk.prev_date);
  }
};
}
}
