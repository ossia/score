#pragma once
#include <QtMath>
#include <Engine/Node/PdNode.hpp>
#include <array>
#include <tuple>
#include <utility>
#include <algorithm>
#include <map>
#include <tuple>
#include <bitset>
#include <random>
#include <ossia/dataflow/execution_state.hpp>
namespace Nodes
{


namespace LFO
{
struct Node
{
    struct Metadata: Control::Meta_base
  {
    static const constexpr auto prettyName = "LFO";
    static const constexpr auto objectKey = "LFO";
    static const constexpr auto category = "Control";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("0697b807-f588-49b5-926c-f97701edd0d8");

    static const constexpr auto value_outs  = ossia::safe_nodes::value_outs<1>{value_out{"out"}};

    static const constexpr auto controls =
        std::make_tuple(Control::Widgets::LFOFreqChooser()
                        , Control::FloatSlider{"Coarse intens.", 0., 1000., 0.}
                        , Control::FloatSlider{"Fine intens.", 0., 1., 1.}
                        , Control::FloatSlider{"Offset.", -1000., 1000., 0.}
                        , Control::FloatSlider{"Jitter", 0., 1., 0.}
                        , Control::FloatSlider{"Phase", -1., 1., 0.}
                        , Control::Widgets::WaveformChooser());
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
      int64_t phase{};
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void run(
      float freq, float coarse, float fine, float offset, float jitter, float phase,
      const std::string& type,
      ossia::value_port& out,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state& st,
      State& s)
  {
    auto& waveform_map = Control::Widgets::waveformMap();

    static std::mt19937 rd;

    if(auto it = waveform_map.find(type); it != waveform_map.end())
    {
      float new_val{};
      auto ph = s.phase;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 5000.)(rd) * jitter;
      }

      using namespace Control::Widgets;
      const auto phi = phase + (2.f * float(M_PI) * freq * ph) / st.sampleRate;

      switch(it->second)
      {
        case Sin:
          new_val = (coarse + fine) * std::sin(phi);
          break;
        case Triangle:
          new_val = (coarse + fine) * std::asin(std::sin(phi));
          break;
        case Saw:
          new_val = (coarse + fine) * std::atan(std::tan(phi));
          break;
        case Square:
          new_val = (coarse + fine) * ((std::sin(phi) > 0.f) ? 1.f : -1.f);
          break;
        case Noise1:
          new_val = std::uniform_real_distribution<float>(-(coarse + fine), coarse + fine)(rd);
          break;
        case Noise2:
          new_val = std::normal_distribution<float>(0, coarse + fine)(rd);
          break;
        case Noise3:
          new_val = std::cauchy_distribution<float>(0, coarse + fine)(rd);
          break;
      }
      out.add_raw_value(new_val + offset);
    }

    s.phase += (tk.date - prev_date);
  }
};
}
}
