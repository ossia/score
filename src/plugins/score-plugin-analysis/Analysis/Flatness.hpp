#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct Flatness : A2::GistState
{
  halp_meta(name, "Spectral flatness")
  halp_meta(c_name, "Flatness")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectral-parameters")
  halp_meta(description, "Get the spectral flatness of a signal")
  halp_meta(uuid, "a2806714-0233-41ed-842b-de6978aac728");
  
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
    process<&Gist<double>::spectralFlatness>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
