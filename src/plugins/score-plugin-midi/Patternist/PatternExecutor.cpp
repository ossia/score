// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>

#include <Patternist/PatternModel.hpp>

namespace Patternist
{
class pattern_node : public ossia::nonowning_graph_node
{
public:
  ossia::midi_outlet out;
  Pattern pattern;
  ossia::flat_set<uint8_t> in_flight;

  int current = 0;
  uint8_t channel{1};

  pattern_node()
  {
    in_flight.container.reserve(32);
    m_outlets.push_back(&out);
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept
  {
    if (auto date = tk.get_physical_quantification_date(pattern.division, st.modelToSamples()))
    {
      auto& mess = out.target<ossia::midi_port>()->messages;
      for (uint8_t note : in_flight)
      {
        mess.push_back(libremidi::message::note_off(channel, note, 0));
        mess.back().timestamp = *date;
      }
      in_flight.clear();

      for (Lane& lane : pattern.lanes)
      {
        if (lane.pattern[current])
        {
          mess.push_back(libremidi::message::note_on(channel, lane.note, 64));
          mess.back().timestamp = *date;
          in_flight.insert(lane.note);
        }
      }
      current = (current + 1) % pattern.length;
    }
  }
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

  con(element, &Patternist::ProcessModel::channelChanged, this, [=](int c) {
    in_exec([=] { node->channel = c; });
  });
  con(element, &Patternist::ProcessModel::currentPatternChanged, this, [=, &element](int c) {
    in_exec([=, p = element.patterns()[c]] { node->pattern = p; });
  });
  con(element, &Patternist::ProcessModel::patternsChanged, this, [=, &element]() {
    in_exec([=, p = element.patterns()[element.currentPattern()]] { node->pattern = p; });
  });
}

Executor::~Executor() { }
}
