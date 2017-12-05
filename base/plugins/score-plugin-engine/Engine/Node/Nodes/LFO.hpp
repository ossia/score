#pragma once
#include <Engine/Node/PdNode.hpp>
#include <array>
#include <tuple>
#include <utility>
#include <algorithm>
#include <map>
#include <tuple>
#include <bitset>
#include <random>
namespace Nodes
{


namespace LFO
{
struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "LFO";
    static const constexpr auto objectKey = "LFO";
    static const constexpr auto category = "Control";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("0697b807-f588-49b5-926c-f97701edd0d8");
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
      int64_t phase{};
  };


  static const constexpr auto info =
      Process::create_node()
      .value_outs({{"out"}})
      .controls(Process::Widgets::LFOFreqChooser()
              , Process::FloatSlider{"Coarse intens.", 0., 1000., 0.}
              , Process::FloatSlider{"Fine intens.", 0., 1., 1.}
              , Process::FloatSlider{"Offset.", -1000., 1000., 0.}
              , Process::FloatSlider{"Jitter", 0., 1., 0.}
              , Process::FloatSlider{"Phase", -1., 1., 0.}
              , Process::Widgets::WaveformChooser()
                )
      .state<State>()
      .build();

  static void run_precise(
      float freq, float coarse, float fine, float offset, float jitter, float phase,
      const std::string& type,
      ossia::value_port& out,
      State& s,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state& st)
  {
    auto& waveform_map = Process::Widgets::waveformMap();

    static std::mt19937 rd;

    if(auto it = waveform_map.find(type); it != waveform_map.end())
    {
      float new_val{};
      auto ph = s.phase;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 5000.)(rd) * jitter;
      }

      using namespace Process::Widgets;
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

using Factories = Process::Factories<Node>;
}
}
