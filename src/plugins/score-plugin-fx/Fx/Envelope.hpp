#pragma once
#include <Fx/Types.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Nodes
{
namespace Envelope
{
struct Node
{
  halp_meta(name, "Envelope")
  halp_meta(c_name, "Envelope")
  halp_meta(category, "Audio")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/analysis.html#envelope")
  halp_meta(description, "Converts an audio signal into RMS and peak values")
  halp_meta(uuid, "95F44151-13EF-4537-8189-0CC243341269");

  using FP = double;
  struct
  {
    halp::dynamic_audio_bus<"in", FP> audio;
  } inputs;
  struct
  {
    halp::callback<"rms", multichannel_output_type> rms;
    halp::callback<"peak", multichannel_output_type> peak;
  } outputs;

  halp_flag(deprecated);

  static auto get(const avnd::span<FP>& chan)
  {
    if(chan.size() > 0)
    {
      auto max = chan[0];
      auto rms = 0.;
      for(auto sample : chan)
      {
        max = std::max(max, std::abs(sample));
        rms += sample * sample;
      }
      rms = std::sqrt(rms);
      rms /= chan.size();

      return std::make_pair(rms, max);
    }
    else
    {
      return std::make_pair(FP{}, FP{});
    }
  }

  void operator()(int d)
  {
    auto& audio = inputs.audio;
    switch(audio.channels)
    {
      case 0:
        return;
      case 1: {
        auto [rms, peak] = get(audio.channel(0, d));
        outputs.rms(rms);
        outputs.peak(peak); // FIXME timestamp
        break;
      }
      default: {
        multichannel_output_vector peak_vec;
        peak_vec.reserve(audio.channels);
        multichannel_output_vector rms_vec;
        rms_vec.reserve(audio.channels);
        for(int i = 0; i < audio.channels; i++)
        {
          auto [rms, peak] = get(audio.channel(i, d));
          rms_vec.push_back(rms);
          peak_vec.push_back(peak);
        }
        outputs.rms(rms_vec);
        outputs.peak(peak_vec); // FIXME timestamp
      }
      break;
    }
  }
};
}
}
