#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct ZeroCrossing : A2::GistState
{
  halp_meta(name, "Zero-crossings")
  halp_meta(c_name, "Zerocross")
  halp_meta(category, "Analysis/Pitch")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#pitch-detection")
  halp_meta(description, "Get the zero-crossing rate of a signal")
  halp_meta(uuid, "2f8f6705-985a-4f36-bb69-ab3c08c8831c");
  
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
    process<&Gist<double>::zeroCrossingRate>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
