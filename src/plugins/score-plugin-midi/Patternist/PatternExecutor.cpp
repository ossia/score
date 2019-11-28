// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <Patternist/PatternModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <ossia/dataflow/port.hpp>

namespace Patternist
{
class pattern_node : public ossia::nonowning_graph_node
{
public:
  ossia::outlet out{ossia::midi_port{}};
  int channel{1};
  Pattern pattern;
  int current = 0;

  pattern_node()
  {
    m_outlets.push_back(&out);
  }

  void run(const ossia::token_request & tk, ossia::exec_state_facade st) noexcept
  {
    if(auto date = tk.get_quantification_date(pattern.division))
    {
      auto& mess = out.data.target<ossia::midi_port>()->messages;
      for(int note : in_flight)
      {
        mess.push_back(rtmidi::message::note_off(channel, note, 0));
      }
      in_flight.clear();

      for(Lane& lane : pattern.lanes)
      {
        if(lane.pattern[current])
        {
          mess.push_back(rtmidi::message::note_on(channel, lane.note, 64));
          mess.back().timestamp = tk.offset.impl * st.modelToSamples() ;
          in_flight.insert(lane.note);
        }
      }
      current = (current+1) % pattern.length;
    }
  }

  ossia::flat_set<int> in_flight;
};

Executor::Executor(
    Patternist::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<Patternist::ProcessModel, ossia::node_process>{
          element,
          ctx,
          id,
          "PatternComponent",
          parent}
{
  auto node = std::make_shared<pattern_node>();
  node->channel = element.channel();
  node->pattern = element.patterns()[element.currentPattern()];

  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
}

Executor::~Executor()
{

}
}
