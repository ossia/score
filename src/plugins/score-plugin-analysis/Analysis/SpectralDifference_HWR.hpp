#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct SpectralDiffHWR : A2::GistState
{
  halp_meta(name, "Spectral Difference (HWR)")
  halp_meta(c_name, "SpectralDiffHWR")
  halp_meta(category, "Analysis/Onsets")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#onset-detection")
  halp_meta(description, "Get the spectral difference (half-wave rectified) of a signal")
  halp_meta(uuid, "9c29887f-e44e-440a-baa1-f0f55a9e57f5");
  
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
