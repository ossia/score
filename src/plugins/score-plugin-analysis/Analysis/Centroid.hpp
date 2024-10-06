#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct Centroid : Analysis::GistState
{
  halp_meta(name, "Centroid")
  halp_meta(c_name, "Centroid")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectral-parameters")
  halp_meta(description, "Get the centroid of a signal")
  halp_meta(uuid, "9d26b429-a417-4c98-a4c6-70af90a5c4ab");

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
    process<&Gist<double>::spectralCentroid>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
