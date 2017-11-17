// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"
#include <Engine/score2OSSIA.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/dataflow/graph.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/state_node.hpp>

namespace Engine
{
namespace Execution
{
SCORE_PLUGIN_ENGINE_EXPORT StateComponent::StateComponent(
    const Scenario::StateModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : Execution::Component{ctx, id, "Executor::State", nullptr}
  , m_state{Engine::score_to_ossia::state(element, ctx)}
{
}

SCORE_PLUGIN_ENGINE_EXPORT void StateComponent::cleanup()
{
  m_ev.reset();
}

SCORE_PLUGIN_ENGINE_EXPORT void StateComponent::onSetup(
    const std::shared_ptr<ossia::time_event>& root)
{
  m_ev = root;
  if(!m_state.empty())
  {
    auto node = std::make_shared<ossia::state_node>(m_state);
    m_ev->add_time_process(
          std::make_shared<ossia::node_process>(
            node));
    this->system().plugin.execGraph->add_node(node);
  }
}

SCORE_PLUGIN_ENGINE_EXPORT void StateComponent::onDelete() const
{
  system().executionQueue.enqueue([gr=this->system().plugin.execGraph,ev=m_ev,st=m_state] {
    auto& procs = ev->get_time_processes();
    if(!procs.empty())
    {
      const auto& proc = (*procs.begin());
      gr->remove_node(proc->node);
      ev->remove_time_process(proc.get());
    }
  });
}

}
}
