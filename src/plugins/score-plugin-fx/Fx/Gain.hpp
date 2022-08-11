#pragma once
#include <Engine/Node/SimpleApi.hpp>

namespace Nodes::Gain
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Gain";
    static const constexpr auto objectKey = "Gain";
    static const constexpr auto category = "Audio";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::AudioEffect;
    static const constexpr auto description = "A simple volume control";
    static const uuid_constexpr auto uuid
        = make_uuid("6c158669-0f81-41c9-8cc6-45820dcda867");

    static const constexpr auto controls
        = tuplet::make_tuple(Control::FloatSlider{"Gain", 0., 2., 1.});
    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::audio_port& p1, float g, ossia::audio_port& p2,
      ossia::token_request t, ossia::exec_state_facade st)
  {
    const auto chans = p1.channels();
    p2.set_channels(chans);

    const auto [first_pos, N] = st.timings(t);

    const double gain = g;
    for(std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& out = p2.channel(i);

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);

      for(int64_t j = first_pos; j < max; j++)
      {
        out[j] = in[j] * gain;
      }
    }
  }
};
}
