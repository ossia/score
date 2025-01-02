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

struct EnvelopeFollower
{
  halp_meta(name, "Envelope Follower (audio)")
  halp_meta(c_name, "EnvelopeFollowerAudio")
  halp_meta(category, "Analysis/Envelope")
  halp_meta(author, "Kevin Ferguson")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#envelope")
  halp_meta(
      description,
      "Sample-level envelope Follower\n"
      "(https://kferg.dev/posts/2020/audio-reactive-programming-envelope-followers/)")
  halp_meta(uuid, "0a262706-1216-44f4-85ca-52f1f25785bc")

  struct inputs
  {
    halp::knob_f32<"Millis (up)", halp::range{0., 1000., 50.}> a;
    halp::knob_f32<"Millis (down)", halp::range{0., 1000., 15.}> b;
  };
  struct outputs
  {
  };
  void prepare(halp::setup s) { rate = s.rate; }
  double rate{48000.};
  double y{};

  double operator()(double x, struct inputs inputs, outputs) noexcept
  {
    using namespace std;
    const auto abs_x = abs(x);
    if(rate > 0)
    {
      const auto a = exp(log(0.5) / (rate * (inputs.a.value / 1000.0)));
      const auto b = exp(log(0.5) / (rate * (inputs.b.value / 1000.0)));

      const auto coeff = (abs_x > y) ? a : b;
      y = coeff * y + (1. - coeff) * abs_x;
    }
    else
    {
      y = 0.;
    }
    return y;
  }
};
}
