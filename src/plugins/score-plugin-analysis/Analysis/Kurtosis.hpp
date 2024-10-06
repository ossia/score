#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct Kurtosis : Analysis::GistState
{
  
  halp_meta(name, "Complex Spectral Difference")
  halp_meta(c_name, "CSD")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectral-parameters")
  halp_meta(description, "Get the kurtosis of a signal")
  halp_meta(uuid, "6e9ed0f9-c541-4c74-b3b7-5d5da77466a0");
  

  struct
  {
    audio_in audio;
    gain_slider gain;
    gate_slider gate;
  } inputs;
  struct
  {
    value_out result;
  } outputs;

  void operator()(int frames)
  {
    process<&Gist<double>::complexSpectralDifference>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
