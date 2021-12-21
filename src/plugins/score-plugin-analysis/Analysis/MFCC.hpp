#pragma once
#include <Engine/Node/SimpleApi.hpp>
#include <Analysis/GistState.hpp>
#include <numeric>
namespace Analysis
{
struct Spectrum
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Spectrum";
    static const constexpr auto objectKey = "Mel";
    static const constexpr auto category = "Analysis/Spectrum";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the magnitude spectrum of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("422a1f92-821c-4073-ae50-e7c21487e27d");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
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
      ossia::audio_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade e,
      State& st)
  {
    st.processVector<&Gist<double>::getMagnitudeSpectrum>(in, gain, gate, out, tk, e);
  }
};

struct MelSpectrum
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Mel spectrum";
    static const constexpr auto objectKey = "Mel";
    static const constexpr auto category = "Analysis/Spectrum";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the Mel frequency spectrum of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("f2b62e47-0e67-476f-b757-ef6a48610a78");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
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
      ossia::audio_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade e,
      State& st)
  {
    st.processVector<&Gist<double>::getMelFrequencySpectrum>(in, gain, gate, out, tk, e);
  }
};

struct MFCC
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "MFCC";
    static const constexpr auto objectKey = "MFCC";
    static const constexpr auto category = "Analysis/Spectrum";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the mel-frequency cepstral coefficients of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("26684acb-36f5-4a8b-8ed3-f32f9ffb436b");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
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
      ossia::audio_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade e,
      State& st)
  {
    st.processVector<&Gist<double>::getMelFrequencyCepstralCoefficients>(in, gain, gate, out, tk, e);
  }
};
}
