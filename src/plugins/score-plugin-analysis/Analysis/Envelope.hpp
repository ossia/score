#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <Analysis/GistState.hpp>

#include <numeric>
namespace Analysis
{
struct RMS
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "RMS";
    static const constexpr auto objectKey = "RMS";
    static const constexpr auto category = "Analysis/Envelope";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the RMS of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("5d4057ff-d8d0-4d66-9e0f-55675e3323be");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr auto controls = tuplet::make_tuple(
        Control::LogFloatSlider{"Gain", 0., 100., 1.},
        Control::FloatSlider{"Gate", 0., 1., 0.});

    static const constexpr value_out value_outs[]{"out"};
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in, float gain, float gate, ossia::value_port& out,
      ossia::token_request tk, ossia::exec_state_facade e, State& st)
  {
    st.process<&Gist<double>::rootMeanSquare>(in, gain, gate, out, tk, e);
  }
};
struct Peak
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Peak";
    static const constexpr auto objectKey = "Peak";
    static const constexpr auto category = "Analysis/Envelope";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the peak energy of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("a14c8ced-25e6-4c89-ac45-63750cbb87fd");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr auto controls = tuplet::make_tuple(
        Control::LogFloatSlider{"Gain", 0., 100., 1.},
        Control::FloatSlider{"Gate", 0., 1., 0.});

    static const constexpr value_out value_outs[]{"out"};
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in, float gain, float gate, ossia::value_port& out,
      ossia::token_request tk, ossia::exec_state_facade e, State& st)
  {
    st.process<&Gist<double>::peakEnergy>(in, gain, gate, out, tk, e);
  }
};
}
