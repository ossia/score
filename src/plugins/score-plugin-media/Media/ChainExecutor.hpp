#pragma once
#include <Media/SynthChain/SynthChainModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_chain_process.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/nodes/dummy.hpp>

#include <fmt/format.h>

namespace ossia
{
class empty_audio_mapper final : public ossia::nonowning_graph_node
{
  ossia::audio_inlet audio_in;
  ossia::audio_outlet audio_out;

public:
  empty_audio_mapper()
  {
    m_inlets.push_back(&audio_in);
    m_outlets.push_back(&audio_out);
  }

  void run(const ossia::token_request&, ossia::exec_state_facade st) noexcept override
  {
    *audio_out = *audio_in;
  }

  // graph_node interface
public:
  std::string label() const noexcept override { return "empty_audio_mapper"; }
};

}
namespace fmt
{
template <>
struct formatter<ossia::graph_node>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const ossia::graph_node& e, FormatContext& ctx)
  {
    return fmt::format_to(ctx.begin(), "{} ({})", e.label(), (void*)&e);
  }
};

template <>
struct formatter<ossia::graph_edge>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const ossia::graph_edge& e, FormatContext& ctx)
  {
    return fmt::format_to(
        ctx.begin(),
        "{}[{}] -> {}[{}]",
        *e.out_node,
        std::distance(
            ossia::find(e.out_node->root_outputs(), e.out), e.out_node->root_outputs().begin()),
        *e.in_node,
        std::distance(
            ossia::find(e.in_node->root_inputs(), e.in), e.in_node->root_inputs().begin()));
  }
};
}

namespace Media
{

static void
check_exec_validity(const ossia::graph_interface& g, const ossia::node_chain_process& proc)
{
  for (auto& node : proc.nodes)
  {
    SCORE_ASSERT(ossia::contains(g.get_nodes(), node.get()));
  }

  for (auto it = proc.nodes.begin(); it != proc.nodes.end(); ++it)
  {
    if (it + 1 == proc.nodes.end())
      break;

    auto& outputs = it->get()->root_outputs();
    SCORE_ASSERT(outputs.size() >= 1);

    auto& inputs = (it + 1)->get()->root_inputs();
    SCORE_ASSERT(inputs.size() >= 1);

    SCORE_ASSERT(outputs.front()->targets.size() == 1);
    SCORE_ASSERT(inputs.front()->sources.size() == 1);

    SCORE_ASSERT(outputs.front()->targets[0] == inputs.front()->sources[0]);
    auto edge = outputs.front()->targets[0];
    SCORE_ASSERT(edge->out_node.get() == it->get());
    SCORE_ASSERT(edge->in_node.get() == (it + 1)->get());
  }
}

static void
check_exec_order(const std::vector<ossia::node_ptr>& g, const ossia::node_chain_process& proc)
{
  SCORE_ASSERT(proc.nodes == g);
}
static void
check_last_validity(const ossia::graph_interface& g, const ossia::node_chain_process& proc)
{
  if (!proc.nodes.empty())
    SCORE_ASSERT(proc.node == proc.nodes.back());
  else
    SCORE_ASSERT(proc.node == std::shared_ptr<ossia::graph_node>{});
}
static auto move_edges(
    ossia::inlet& old_in,
    ossia::inlet_ptr new_in,
    std::shared_ptr<ossia::graph_node> new_node,
    ossia::graph_interface& g)
{
  auto old_sources = old_in.sources;
  for (ossia::graph_edge* e : old_sources)
  {
    g.connect(ossia::make_edge(e->con, e->out, new_in, e->out_node, std::move(new_node)));
    g.disconnect(e);
  }
}
static auto move_edges(
    ossia::outlet& old_out,
    ossia::outlet_ptr new_out,
    std::shared_ptr<ossia::graph_node> new_node,
    ossia::graph_interface& g)
{
  auto old_targets = old_out.targets;
  for (ossia::graph_edge* e : old_targets)
  {
    g.connect(ossia::make_edge(e->con, new_out, e->in, std::move(new_node), e->in_node));
    g.disconnect(e);
  }
}

template <typename RegisteredEffect_T>
static std::vector<ossia::node_ptr>
get_nodes(const std::vector<std::pair<Id<Process::ProcessModel>, RegisteredEffect_T>>& fx)
{
  std::vector<ossia::node_ptr> v;
  for (const auto& f : fx)
  {
    SCORE_ASSERT(f.second);
    v.push_back(f.second.node());
  }
  return v;
}

class DummyProcessComponent : public Execution::ProcessComponent
{
  COMPONENT_METADATA("2875d036-b9d0-4b43-aa9d-926bb2902edc")
public:
  DummyProcessComponent(
      Process::ProcessModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : Execution::ProcessComponent{element, ctx, id, "Dummy", parent}
  {
    auto N_i = element.inlets().size();
    auto N_o = element.outlets().size();
    if (N_i == 0 || N_o == 0)
      return;

    auto& i = element.inlets().front();
    auto& o = element.outlets().front();
    std::size_t index_i = 0;
    std::size_t index_o = 0;
    if (i->type() != o->type())
    {
      this->node = std::make_shared<ossia::nodes::dummy_node>();
    }
    else
    {
      switch (i->type())
      {
        case Process::PortType::Audio:
          this->node = std::make_shared<ossia::nodes::dummy_audio_node>();
          break;
        case Process::PortType::Midi:
          this->node = std::make_shared<ossia::nodes::dummy_midi_node>();
          break;
        case Process::PortType::Message:
          this->node = std::make_shared<ossia::nodes::dummy_value_node>();
          break;
      }
      index_i = 1;
      index_o = 1;
    }

    for (; index_i < N_i; index_i++)
    {
      switch (element.inlets()[index_i]->type())
      {
        case Process::PortType::Audio:
          this->node->root_inputs().push_back(new ossia::audio_inlet);
          break;
        case Process::PortType::Midi:
          this->node->root_inputs().push_back(new ossia::midi_inlet);
          break;
        case Process::PortType::Message:
          this->node->root_inputs().push_back(new ossia::value_inlet);
          break;
      }
    }
    for (; index_o < N_o; index_o++)
    {
      switch (element.inlets()[index_o]->type())
      {
        case Process::PortType::Audio:
          this->node->root_outputs().push_back(new ossia::audio_outlet);
          break;
        case Process::PortType::Midi:
          this->node->root_outputs().push_back(new ossia::midi_outlet);
          break;
        case Process::PortType::Message:
          this->node->root_outputs().push_back(new ossia::value_outlet);
          break;
      }
    }

    m_ossia_process = std::make_shared<ossia::node_process>(node);
  }
};

}
