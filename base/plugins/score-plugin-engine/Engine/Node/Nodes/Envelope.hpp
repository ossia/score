#include <Engine/Node/PdNode.hpp>
#include <numeric>
namespace Nodes
{
namespace Envelope
{
struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "Envelope";
    static const constexpr auto objectKey = "Envelope";
    static const constexpr auto category = "Audio";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("95F44151-13EF-4537-8189-0CC243341269");
  };

  static const constexpr auto info =
      Process::create_node()
      .audio_ins({{"audio"}})
      .value_outs({{"rms"}, {"peak"}})
      .build();

  static auto get(const ossia::audio_channel& chan)
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
      using val_t = ossia::audio_channel::value_type;
      return std::make_pair(val_t{}, val_t{});
    }
  }

  static void run(
      const ossia::audio_port& audio,
      ossia::value_port& rms_port,
      ossia::value_port& peak_port,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state&)
  {
    switch(audio.samples.size())
    {
      case 0:
        return;
      case 1:
      {
        auto [rms, peak] = get(audio.samples[0]);

        rms_port.add_value(rms);
        peak_port.add_value(peak);
        break;
      }
      default:
      {
          std::vector<ossia::value> peak_vec;
          peak_vec.reserve(audio.samples.size());
          std::vector<ossia::value> rms_vec;
          rms_vec.reserve(audio.samples.size());
          for(auto& c : audio.samples)
          {
            auto [rms, peak] = get(c);
            rms_vec.push_back(rms);
            peak_vec.push_back(peak);
          }
          rms_port.add_value(rms_vec);
          peak_port.add_value(peak_vec);

      }
        break;
    }
  }

};

using Factories = Process::Factories<Node>;
}
}
