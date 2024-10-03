#pragma once
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#if defined(OSSIA_ENABLE_KFR)
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#endif

namespace A2
{
#if defined(OSSIA_ENABLE_KFR)
struct PitchState : A2::GistState
{
  PitchState()
      : hipass{kfr::to_sos(
            kfr::iir_highpass(kfr::butterworth<double>(12), 200, this->rate))}
  {
  }

  void filter(halp::dynamic_audio_bus_base<double>& in, int d)
  {
    while(hipass.size() < in.channels)
    {
      hipass.emplace_back(
          kfr::to_sos(kfr::iir_highpass(kfr::butterworth<double>(12), 200, this->rate)));
    }

    int c = 0;
    for(double* chan : in)
    {
      hipass[c++].apply(chan, d);
    }
  }

  using hipass_t = decltype(kfr::to_sos(
      kfr::iir_highpass(kfr::zpk<double>{}, kfr::identity<double>{})));
  std::vector<kfr::iir_filter<double>> hipass;
};
#else
using PitchState = GistState;
#endif

struct Pitch : PitchState
{
  halp_meta(name, "Pitch detector")
  halp_meta(c_name, "Pitch")
  halp_meta(category, "Analysis/Pitch")
  halp_meta(author, "ossia score, Gist library")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#pitch-detection")
  halp_meta(description, "Get the pitch of a signal")
  halp_meta(uuid, "ed511605-8265-4b2c-8c4b-d3b189539b3b");
  
  struct
  {
    audio_in audio;
  } inputs;
  struct
  {
    value_out result;
  } outputs;

  void operator()(int frames)
  {
    if(inputs.audio.channels == 0)
      return;

#if defined(OSSIA_ENABLE_KFR)
    filter(inputs.audio, frames);
#endif
    process<&Gist<double>::pitch>(inputs.audio, outputs.result, frames);
  }
};
}
