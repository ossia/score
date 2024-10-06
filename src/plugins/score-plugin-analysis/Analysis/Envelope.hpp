#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct RMS : Analysis::GistState
{
  halp_meta(name, "RMS")
  halp_meta(c_name, "RMS")
  halp_meta(category, "Analysis/Envelope")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#envelope")
  halp_meta(description, "Get the RMS of a signal")
  halp_meta(uuid, "5d4057ff-d8d0-4d66-9e0f-55675e3323be");

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
    process<&Gist<double>::rootMeanSquare>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};

struct Peak : Analysis::GistState
{
  halp_meta(name, "Peak")
  halp_meta(c_name, "Peak")
  halp_meta(category, "Analysis/Envelope")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#envelope")
  halp_meta(description, "Get the peak energy of a signal")
  halp_meta(uuid, "a14c8ced-25e6-4c89-ac45-63750cbb87fd")

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
    process<&Gist<double>::peakEnergy>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
