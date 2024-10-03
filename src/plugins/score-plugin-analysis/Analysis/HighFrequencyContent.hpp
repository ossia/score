#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct HFQ : A2::GistState
{
  halp_meta(name, "High-Frequency Content")
  halp_meta(c_name, "Hfq")
  halp_meta(category, "Analysis/Onsets")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#onset-detection")
  halp_meta(description, "Get the high-frequency content of a signal")
  halp_meta(uuid, "75f12985-63b6-4dc1-946f-a65a3dc54eed");
  
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
    process<&Gist<double>::highFrequencyContent>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, outputs.pulse, frames);
  }
};
}
