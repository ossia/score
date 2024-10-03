#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace A2
{
struct Rolloff : A2::GistState
{
  halp_meta(name, "Rolloff")
  halp_meta(c_name, "Rolloff")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectral-parameters")
  halp_meta(description, "Get the complex spectral rolloff of a signal")
  halp_meta(uuid, "fd659287-9848-4190-907d-4be3f0df2c4b");
  
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
    process<&Gist<double>::spectralRolloff>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
