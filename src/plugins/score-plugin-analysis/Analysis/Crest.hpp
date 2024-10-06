#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct Crest : Analysis::GistState
{
  halp_meta(name, "Spectral Crest")
  halp_meta(c_name, "Crest")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectral-parameters")
  halp_meta(description, "Get the spectral crest of a signal")
  halp_meta(uuid, "41755e45-dc1f-4bd8-b2c9-6a455119339a");
  

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
    process<&Gist<double>::spectralCrest>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
