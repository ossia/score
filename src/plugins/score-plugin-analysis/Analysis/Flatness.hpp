#pragma once
#include <Engine/Node/SimpleApi.hpp>
#include <Analysis/GistState.hpp>
#include <numeric>
namespace Analysis
{
struct Flatness
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Spectral flatness";
    static const constexpr auto objectKey = "Flatness";
    static const constexpr auto category = "Analysis/Spectrum";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the spectral flatness of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("a2806714-0233-41ed-842b-de6978aac728");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
    static const constexpr auto controls = tuplet::make_tuple(
          Control::LogFloatSlider{"Gain", 0., 100., 1.},
          Control::FloatSlider{"Gate", 0., 1., 0.}
    );
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in,
      float gain,
      float gate,
      ossia::value_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade e,
      State& st)
  {
    st.process<&Gist<double>::spectralFlatness>(in, gain, gate, out, tk, e);
  }
};
}
