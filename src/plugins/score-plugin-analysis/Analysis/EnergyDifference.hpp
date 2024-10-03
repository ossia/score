#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct EnergyDifference : A2::GistState
{
  halp_meta(name, "Energy Difference")
  halp_meta(c_name, "EnergyDifference")
  halp_meta(category, "Analysis/Onsets")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#onset-detection")
  halp_meta(description, "Get the energy difference of a signal. Detects onsets.")
  halp_meta(uuid, "1c15b7d4-fa06-4eb2-b59f-39758308d4f8");
  

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
    process<&Gist<double>::energyDifference>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, outputs.pulse, frames);
  }
};
}
