#pragma once
#include <Engine/Node/PdNode.hpp>
namespace Nodes
{
namespace Direction
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Angle mapper";
    static const constexpr auto objectKey = "AngleMapper";
    static const constexpr auto category = "Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Map the variation of an angle";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("9b0e21ba-965a-4aa4-beeb-60cc5128c418");

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

  struct State
  {
    ossia::value prev_value{};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::value_port& p1,
      ossia::value_port& p2,
      ossia::token_request,
      ossia::exec_state_facade,
      State& self)
  {
    // returns -1, 0, 1 to say if we're going backwards, staying equal, or
    // going forward.

    for (const auto& val : p1.get_data())
    {
      if (!self.prev_value.valid())
      {
        self.prev_value = val.value;
        continue;
      }
      else
      {
        ossia::value output;

        if (self.prev_value < val.value)
        {
          output = 1;
          self.prev_value = val.value;
        }
        else if (self.prev_value > val.value)
        {
          output = -1;
          self.prev_value = val.value;
        }
        else
        {
          output = 0;
        }

        p2.write_value(std::move(output), val.timestamp);
      }
    }
  }
};
}
}
