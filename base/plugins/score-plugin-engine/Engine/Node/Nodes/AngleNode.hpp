#include <Engine/Node/PdNode.hpp>
namespace Nodes
{
namespace Direction
{
struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "Angle mapper";
    static const constexpr auto objectKey = "AngleMapper";
    static const constexpr auto category = "Mappings";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("9b0e21ba-965a-4aa4-beeb-60cc5128c418");
  };
  struct State
  {
    ossia::value prev_value{};
  };

  static const constexpr auto info =
      Process::create_node()
      .value_ins({{"in"}})
      .value_outs({{"out"}})
      .state<State>()
      .build();

  static void run(
      const ossia::value_port& p1,
      ossia::value_port& p2,
      State& self,
      ossia::time_value prev_date,
      ossia::token_request,
      ossia::execution_state&)
  {
    // returns -1, 0, 1 to say if we're going backwards, staying equal, or going forward.

    for(const auto& val : p1.get_data())
    {
      if(!self.prev_value.valid())
      {
        self.prev_value = val.value;
        continue;
      }
      else
      {
        ossia::tvalue output;
        output.timestamp = val.timestamp;

        if(self.prev_value < val.value)
        {
          output.value = 1;
          self.prev_value = val.value;
        }
        else if(self.prev_value > val.value)
        {
          output.value = -1;
          self.prev_value = val.value;
        }
        else
        {
          output.value = 0;
        }

        p2.add_value(output);
      }
    }
  }
};

using Factories = Process::Factories<Node>;
}

}
