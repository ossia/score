#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <Analysis/GistState.hpp>

#include <numeric>
namespace Analysis
{
struct CSD
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Complex Spectral Difference";
    static const constexpr auto objectKey = "CSD";
    static const constexpr auto category = "Analysis/Onsets";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description
        = "Get the complex spectral difference of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("a542f819-e062-4f52-8c54-7e49a9bad5b8");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out", "pulse"};
    static const constexpr auto controls = tuplet::make_tuple(
        Control::LogFloatSlider{"Gain", 0., 100., 1.},
        Control::FloatSlider{"Gate", 0., 1., 0.});
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in, float gain, float gate, ossia::value_port& out,
      ossia::value_port& pulse, ossia::token_request tk, ossia::exec_state_facade e,
      State& st)
  {
    st.process<&Gist<double>::complexSpectralDifference>(
        in, gain, gate, out, pulse, tk, e);
  }
};
}
