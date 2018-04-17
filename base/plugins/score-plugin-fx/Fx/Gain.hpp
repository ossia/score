#pragma once
#include <Engine/Node/PdNode.hpp>
#if defined(__AVX2__) && __has_include(<immintrin.h>)
#include <immintrin.h>
#endif
namespace Nodes::Gain
{
struct Node
{
  struct Metadata: Control::Meta_base
  {
    static const constexpr auto prettyName = "Gain";
    static const constexpr auto objectKey = "Gain";
    static const constexpr auto category = "Audio";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("6c158669-0f81-41c9-8cc6-45820dcda867");

    static const constexpr auto controls = std::make_tuple(Control::FloatSlider{"Gain", 0., 2., 1.});
    static const constexpr auto audio_ins  = ossia::safe_nodes::audio_ins<1>{{"in"}};
    static const constexpr auto audio_outs = ossia::safe_nodes::audio_outs<1>{{"out"}};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void run(
      const ossia::audio_port& p1,
      float g,
      ossia::audio_port& p2,
      ossia::time_value prev_date,
      ossia::token_request,
      ossia::execution_state&)
  {
    const auto chans = p1.samples.size();
    p2.samples.resize(chans);
    for(std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      const auto samples = in.size();
      out.resize(samples);

      std::size_t j = 0;

#if defined(__AVX2__) && __has_include(<immintrin.h>)
      if(samples > 4) {
        const auto gain = _mm256_set1_pd(g);
        for(; j < samples - 4; j += 4)
        {
          _mm256_store_pd(&out[j], _mm256_mul_pd(_mm256_load_pd(&in[j]), gain));
        }
      }
#endif

      {
        for(; j < samples; j++)
        {
          out[j] = in[j] * g;
        }
      }
    }
  }
};
}
