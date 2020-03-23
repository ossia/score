#pragma once
#include <Engine/Node/PdNode.hpp>
namespace Nodes
{
namespace ClassicalBeat
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Impulse Metronome";
    static const constexpr auto objectKey = "ImpulseMetronome";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description
        = "A simple metronome - outputs a bang on the current tick";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("1c185139-04f9-492f-8b4a-000dd4428990");

    static const constexpr value_out value_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(ossia::value_port& res,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    using namespace ossia;
    if (tk.forward())
    {
      tk.metronome(
            st.modelToSamples(),
            [&] (int64_t start_sample) { res.write_value(ossia::impulse{}, start_sample); },
            [&] (int64_t start_sample) { res.write_value(ossia::impulse{}, start_sample); }
      );
    }
  }
};
}
}
