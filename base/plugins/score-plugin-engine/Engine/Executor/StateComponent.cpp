// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"
#include <Engine/score2OSSIA.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <ossia/editor/scenario/time_event.hpp>

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
  m_ev->add_state(m_state);
}

SCORE_PLUGIN_ENGINE_EXPORT void StateComponent::onDelete() const
{
  system().executionQueue.enqueue([ev=m_ev,st=m_state] {
    if(ev)
      ev->remove_state(st);
  });
}

}
}
