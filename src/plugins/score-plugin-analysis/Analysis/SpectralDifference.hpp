#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct SpectralDiff : A2::GistState
{
  halp_meta(name, "Spectral Difference")
  halp_meta(c_name, "SpectralDiff")
  halp_meta(category, "Analysis/Onsets")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#onset-detection")
  halp_meta(description, "Get the spectral difference of a signal")
  halp_meta(uuid, "0871dd59-c77c-45c9-b229-0ff69dd211e3");
  
  struct
  {
    audio_in audio;
    gain_slider gain;
    gate_slider gate;
  } inputs;

  struct
  {
    value_out result;
    pulse_out pulse;
  } outputs;

  void operator()(int frames)
  {
    process<&Gist<double>::spectralDifference>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, outputs.pulse, frames);
  }
};
}
