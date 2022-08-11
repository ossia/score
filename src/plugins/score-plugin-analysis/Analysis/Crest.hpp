#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <Analysis/GistState.hpp>

#include <numeric>
namespace Analysis
{
struct Crest
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Spectral Crest";
    static const constexpr auto objectKey = "Crest";
    static const constexpr auto category = "Analysis/Spectrum";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the spectral crest of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("41755e45-dc1f-4bd8-b2c9-6a455119339a");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
    static const constexpr auto controls = tuplet::make_tuple(
        Control::LogFloatSlider{"Gain", 0., 100., 1.},
        Control::FloatSlider{"Gate", 0., 1., 0.});
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in, float gain, float gate, ossia::value_port& out,
      ossia::token_request tk, ossia::exec_state_facade e, State& st)
  {
    st.process<&Gist<double>::spectralCrest>(in, gain, gate, out, tk, e);
  }
};
}
