#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct CSD : Analysis::GistState
{
  halp_meta(name, "Complex Spectral Difference")
  halp_meta(c_name, "CSD")
  halp_meta(category, "Analysis/Onsets")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#onset-detection")
  halp_meta(description, "Get the complex spectral difference of a signal")
  halp_meta(uuid, "a542f819-e062-4f52-8c54-7e49a9bad5b8");
  

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
    process<&Gist<double>::complexSpectralDifference>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, outputs.pulse, frames);
  }
};
}
