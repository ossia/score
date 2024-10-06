#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Analysis
{
struct Spectrum : Analysis::GistState
{
  halp_meta(name, "Spectrum")
  halp_meta(c_name, "Spectrum")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectrum-extraction")
  halp_meta(description, "Get the magnitude spectrum of a signal")
  halp_meta(uuid, "422a1f92-821c-4073-ae50-e7c21487e27d");

  struct
  {
    audio_in audio;
    gain_slider gain;
    gate_slider gate;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"spectrum", double> result;
  } outputs;

  void operator()(int frames)
  {
    processVector<&Gist<double>::getMagnitudeSpectrum>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};

struct MelSpectrum : Analysis::GistState
{

  halp_meta(name, "Complex Spectral Difference")
  halp_meta(c_name, "CSD")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectrum-extraction")
  halp_meta(description, "Get the Mel frequency spectrum of a signal")
  halp_meta(uuid, "f2b62e47-0e67-476f-b757-ef6a48610a78");

  struct
  {
    audio_in audio;
    gain_slider gain;
    gate_slider gate;
  } inputs;

  struct
  {
    audio_out result;
  } outputs;

  void operator()(int frames)
  {
    processVector<&Gist<double>::getMelFrequencySpectrum>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};

struct MFCC : Analysis::GistState
{

  halp_meta(name, "MFCC")
  halp_meta(c_name, "MFCC")
  halp_meta(category, "Analysis/Spectrum")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#spectrum-extraction")
  halp_meta(description, "Get the mel-frequency cepstral coefficients of a signal")
  halp_meta(uuid,"26684acb-36f5-4a8b-8ed3-f32f9ffb436b");

  struct
  {
    audio_in audio;
    gain_slider gain;
    gate_slider gate;
  } inputs;

  struct
  {
    audio_out result;
  } outputs;

  void operator()(int frames)
  {
    processVector<&Gist<double>::getMelFrequencyCepstralCoefficients>(
        inputs.audio, inputs.gain, inputs.gate, outputs.result, frames);
  }
};
}
