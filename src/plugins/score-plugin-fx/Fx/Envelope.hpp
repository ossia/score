#pragma once
#include <Engine/Node/PdNode.hpp>

#include <numeric>
namespace Nodes
{
namespace Envelope
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Envelope";
    static const constexpr auto objectKey = "Envelope";
    static const constexpr auto category = "Audio";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Converts an audio signal into RMS and peak values";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("95F44151-13EF-4537-8189-0CC243341269");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"rms", "peak"};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static auto get(const ossia::audio_channel& chan)
  {
    if (chan.size() > 0)
    {
      auto max = chan[0];
      auto rms = 0.;
      for (auto sample : chan)
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
      using val_t = ossia::audio_channel::value_type;
      return std::make_pair(val_t{}, val_t{});
    }
  }

  static void
  run(const ossia::audio_port& audio,
      ossia::value_port& rms_port,
      ossia::value_port& peak_port,
      ossia::token_request tk,
      ossia::exec_state_facade e)
  {
    switch (audio.samples.size())
    {
      case 0:
        return;
      case 1:
      {
        auto [rms, peak] = get(audio.samples[0]);

        rms_port.write_value(rms, e.physical_start(tk));
        peak_port.write_value(peak, e.physical_start(tk));
        break;
      }
      default:
      {
        std::vector<ossia::value> peak_vec;
        peak_vec.reserve(audio.samples.size());
        std::vector<ossia::value> rms_vec;
        rms_vec.reserve(audio.samples.size());
        for (auto& c : audio.samples)
        {
          auto [rms, peak] = get(c);
          rms_vec.push_back(rms);
          peak_vec.push_back(peak);
        }
        rms_port.write_value(rms_vec, e.physical_start(tk));
        peak_port.write_value(peak_vec, e.physical_start(tk));
      }
      break;
    }
  }
};
}
}
